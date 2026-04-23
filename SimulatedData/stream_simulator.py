#!/usr/bin/env python3
"""
combined_stream_simulator.py

Combines the EMG simulation scenarios of stream_simulator-1.py with the 
threaded TX/RX and logging approach of stream_simulator.py.
"""

import sys
import time
import random
import math
import argparse
import threading
import os
from datetime import datetime

try:
    import serial
except ImportError:
    serial = None

# Global flag to cleanly shut down threads
is_running = True

# ── Configuration Defaults ───────────────────────────────────────────
DEFAULT_RATE_HZ = 20       # Matches project spec (20 Hz simulation rate)
DEFAULT_DURATION = 0       # 0 means infinite by default
DEFAULT_BAUD = 115200      # From stream_simulator.py
DEFAULT_PORT = 'COM3'      # From stream_simulator.py


# ── Scenario generators ─────────────────────────────────────────────
def scenario_normal(t):
    """Normal driving — low EMG baseline with slight noise."""
    emg = 0.05 + 0.03 * math.sin(2 * math.pi * 0.5 * t) + random.gauss(0, 0.01)
    eda = 0.20 + 0.05 * math.sin(2 * math.pi * 0.1 * t) + random.gauss(0, 0.01)
    return max(0.0, emg), max(0.0, eda), True


def scenario_braking(t):
    """
    Emergency braking sequence:
        0–5s    normal driving
        5–6s    ramp up (muscle contraction begins)
        6–10s   sustained high EMG (braking intent)
        10–12s  ramp down (foot releasing)
        12–20s  return to normal
        20–25s  second braking event
        25+     normal
    """
    if t < 5.0:
        emg = 0.05 + random.gauss(0, 0.01)
    elif t < 6.0:
        progress = (t - 5.0) / 1.0
        emg = 0.05 + 0.65 * progress + random.gauss(0, 0.02)
    elif t < 10.0:
        emg = 0.70 + 0.10 * math.sin(2 * math.pi * 2 * t) + random.gauss(0, 0.03)
    elif t < 12.0:
        progress = (t - 10.0) / 2.0
        emg = 0.70 - 0.65 * progress + random.gauss(0, 0.02)
    elif t < 20.0:
        emg = 0.05 + random.gauss(0, 0.01)
    elif t < 21.0:
        progress = (t - 20.0) / 1.0
        emg = 0.05 + 0.75 * progress + random.gauss(0, 0.02)
    elif t < 25.0:
        emg = 0.80 + random.gauss(0, 0.03)
    else:
        emg = 0.05 + random.gauss(0, 0.01)

    eda = 0.20 + (0.30 if 5.0 < t < 12.0 or 20.0 < t < 26.0 else 0.0)
    eda += random.gauss(0, 0.02)

    return max(0.0, emg), max(0.0, eda), True


def scenario_noisy(t):
    """Noisy data — random spikes, occasional missing frames."""
    emg, eda, _ = scenario_braking(t)

    # 10% chance of noise spike
    if random.random() < 0.10:
        emg += random.uniform(0.3, 0.8)

    # 5% chance of missing frame
    if random.random() < 0.05:
        return 0.0, 0.0, False

    return max(0.0, emg), max(0.0, eda), True


def scenario_fault(t):
    """Sensor fault injection — malformed frames and data gaps."""
    emg, eda, _ = scenario_normal(t)

    # 15% chance of producing a malformed frame
    if random.random() < 0.15:
        return emg, eda, "malformed"

    # 10% chance of gap (no output)
    if random.random() < 0.10:
        return 0.0, 0.0, False

    return emg, eda, True


SCENARIOS = {
    "normal":  scenario_normal,
    "braking": scenario_braking,
    "noisy":   scenario_noisy,
    "fault":   scenario_fault,
    "full":    None,  # Combined: normal → braking → noisy → fault
}


# ── Frame formatting ─────────────────────────────────────────────────
def format_frame(emg, eda, ts_ms, valid):
    """Format a data frame string."""
    if valid == "malformed":
        # Produce various kinds of malformed frames
        patterns = [
            f"{random.uniform(99.0, 999.0):.3f}",
            "",
        ]
        return random.choice(patterns)

    if not valid:
        return None  # Skip this frame

    return f"{emg:.3f}"


# ── Transmission Thread ─────────────────────────────────────────────
def simulate_calf_emg_stream(ser, args, log_filepath):
    """Thread function to send simulated gastrocnemius EMG data to STM32."""
    global is_running
    
    print(f"🚀 Started sending EMG data at {args.rate} Hz using '{args.scenario}' scenario...")
    
    if args.scenario == "full":
        generators = [scenario_normal, scenario_braking, scenario_noisy, scenario_fault]
    else:
        generators = [SCENARIOS[args.scenario]]

    interval = 1.0 / args.rate
    start_time = time.time()
    sample_count = 0

    with open(log_filepath, 'w', encoding='utf-8') as log_file:
        while is_running:
            elapsed = time.time() - start_time
            if args.duration > 0 and elapsed >= args.duration:
                print(f"\n✅ Finished TX scenario duration ({args.duration}s).")
                # Break the transmit loop, but leave main RX loop running if user wants
                break

            if args.scenario == "full":
                # Change scenario every 30 seconds to cycle through all of them
                gen_idx = int(elapsed / 30.0) % len(generators)
                gen = generators[gen_idx]
                local_t = elapsed % 30.0
                emg, eda, valid = gen(local_t)
            else:
                gen = generators[0]
                emg, eda, valid = gen(elapsed)

            ts_ms = int(elapsed * 1000)

            frame = format_frame(emg, eda, ts_ms, valid)
            if frame is not None:
                if ser:
                    ser.write((frame + "\n").encode("utf-8"))
                else:
                    # If no serial port, output to stdout
                    print(f"TX: {frame}", flush=True)

                log_file.write(f"{ts_ms},{frame}\n")
                log_file.flush()

                sample_count += 1

            time.sleep(interval)
        
    print(f"✅ TX thread done. Sent {sample_count} frames.")


# ── Main ─────────────────────────────────────────────────────────────
def main():
    global is_running
    
    parser = argparse.ArgumentParser(
        description="Combined EMG/EDA UART stream simulator with threaded RX logging."
    )
    parser.add_argument("--scenario", choices=SCENARIOS.keys(), default="full",
                        help="Data scenario to simulate (default: full)")
    parser.add_argument("--duration", type=float, default=DEFAULT_DURATION,
                        help=f"Duration in seconds (default: {DEFAULT_DURATION})")
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE_HZ,
                        help=f"Samples per second (default: {DEFAULT_RATE_HZ})")
    parser.add_argument("--serial", type=str, default=DEFAULT_PORT,
                        help=f"Serial port (e.g. COM3, /dev/ttyUSB0). Set to 'None' for stdout only. (default: {DEFAULT_PORT})")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD,
                        help=f"Baud rate for serial output (default: {DEFAULT_BAUD})")
    args = parser.parse_args()

    # Get the exact directory where script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Create the full path for the CSV
    filename = datetime.now().strftime("telemetry_log_%Y%m%d_%H%M%S.csv")
    log_filepath = os.path.join(script_dir, filename)

    ser = None
    tx_thread = None

    try:
        if args.serial and args.serial.lower() != 'none':
            if serial is None:
                print("❌ pyserial not installed. Run: pip install pyserial", file=sys.stderr)
                return 1
            ser = serial.Serial(args.serial, args.baud, timeout=0.1)
            print(f"✅ Connected to {args.serial} at {args.baud} baud.")
        else:
            print("⚠️ No serial port provided or selected. Operating in stdout mode.")

        print(f"▶ Scenario: {args.scenario} | Duration: {args.duration}s | Rate: {args.rate} Hz")

        # Start the transmission thread
        tx_thread = threading.Thread(target=simulate_calf_emg_stream, args=(ser, args, log_filepath))
        tx_thread.start()

        print(f"💾 Logging incoming telemetry to: {log_filepath}")
        print("Press Ctrl+C to stop...\n")

        # Open the CSV file and start listening in the main thread
        while is_running:
            if ser and ser.in_waiting > 0:
                try:
                    raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if raw_line:
                        print(f"RX: {raw_line}")
                        
                except Exception as e:
                    print(f"Data read error: {e}")
            else:
                time.sleep(0.01) # small sleep to prevent CPU hogging
                # Break gracefully if not writing to serial and thread ended
                if not ser and not tx_thread.is_alive():
                    is_running = False
                    print("Simulation complete in stdout mode.")

    except Exception as e:
        print(f"❌ Error during operation: {e}")
    except KeyboardInterrupt:
        print("\n🛑 Stopping simulation...")
    finally:
        is_running = False
        if tx_thread and tx_thread.is_alive():
            tx_thread.join() # Wait for the TX thread to finish cleanly
        if ser and ser.is_open:
            ser.close()
        print("Simulation stopped safely. Log saved.")

    return 0


if __name__ == "__main__":
    sys.exit(main())

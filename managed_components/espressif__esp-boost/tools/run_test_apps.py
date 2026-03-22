# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import serial
import time
import sys

SUCCESS_RESPONSE = '0 Failures'
FAILURE_RESPONSE = '1 Failures'
ENTER_RESPONSE = 'Enter test for running'
REBOOT_RESPONSE = 'Rebooting...'
TIMEOUT_S = 120
RETRY_LIMIT = 4  # Retry once before recording failure


def open_serial(port, baudrate=115200, timeout=1):
    """Open serial port"""
    try:
        set = serial.Serial(port, baudrate, timeout=timeout)
        print(f'Successfully opened serial port {port} at {baudrate} baud rate.')
        return set
    except serial.SerialException as e:
        print(f'Failed to open serial port {port}: {e}')
        return None


def send_enter_command(set):
    """Send enter command"""
    message = '\n\n'
    print(f'[Menu] Sending enter command')
    set.write(message.encode())
    while True:
        response = set.readline().decode().strip()
        print(f'[Menu] Received: {response}')
        if ENTER_RESPONSE in response:
            break
        time.sleep(0.1)


def send_and_wait(set, port, start, end, custom_string):
    """Send numbers and wait for response, handling failures and timeouts"""
    failed_numbers = []  # Store numbers that failed or timed out

    for num in range(start, end + 1):
        retries = 0  # Track retry attempts
        success = False

        while not success and retries < RETRY_LIMIT:
            message = f'{num}\n'
            print(f'[{num}]: Sent (Attempt {retries + 1})')
            set.write(message.encode())

            start_time = time.time()

            while True:
                response = set.readline().decode().strip()
                if response:
                    print(f'[{num}] Received: {response}')

                # Detect custom failure string
                if custom_string and custom_string in response:
                    print(f"[{num}]: Failed (Custom String Detected: '{custom_string}')")
                    retries += 1
                    break  # Retry or move to the next number

                # Detect standard failure message
                if FAILURE_RESPONSE in response:
                    print(f'[{num}]: Failed (Standard Failure)')
                    retries += 1
                    break  # Retry or move to the next number

                # Check for expected response
                if SUCCESS_RESPONSE in response:
                    print(f'[{num}]: Passed')
                    success = True
                    break  # Move to the next number

                # Timeout (60 seconds)
                if time.time() - start_time > TIMEOUT_S:
                    print(f'[{num}]: Timeout! Ending test due to timeout.')
                    failed_numbers.append(num)
                    print(f'[{num}]: Test failed due to timeout.')
                    return failed_numbers  # Exit immediately after timeout

                if REBOOT_RESPONSE in response:
                    failed_numbers.append(num)
                    print(f'[{num}]: Test failed due to reboot.')
                    return failed_numbers

                time.sleep(0.1)  # Prevent high CPU usage

            # If retried and still fails, record the number
            if retries == RETRY_LIMIT:
                failed_numbers.append(num)
                print(f'[{num}]: Marked as failed after {RETRY_LIMIT} attempts.')
                print(f'All failed numbers: {failed_numbers}')
                break  # Move to the next number

        time.sleep(1)  # Short delay before next number

    return failed_numbers  # Return all failed or timed-out numbers


if __name__ == '__main__':
    if len(sys.argv) < 4 or len(sys.argv) > 5:
        print('Usage: python script.py <serial_port> <start_number> <end_number> [custom_failure_string]')
        sys.exit(1)

    port = sys.argv[1]
    try:
        start = int(sys.argv[2])
        end = int(sys.argv[3])
    except ValueError:
        print('Error: Start and end numbers must be integers.')
        sys.exit(1)

    if start > end:
        print('Error: Start number must be less than or equal to the end number.')
        sys.exit(1)

    custom_string = sys.argv[4] if len(sys.argv) == 5 else None

    set = open_serial(port)
    send_enter_command(set)

    if set:
        try:
            failed_numbers = send_and_wait(set, port, start, end, custom_string)
        finally:
            set.close()
            print('Serial port closed.')

        # Print all failed or timed-out numbers and write to file
        if failed_numbers:
            print('\nThe following numbers failed or timed out:')
            print(', '.join(map(str, failed_numbers)))

            # Write failed numbers to file
            import os

            # Create directory if it doesn't exist
            os.makedirs('./build', exist_ok=True)

            # Append to file if it exists, create if it doesn't
            with open('./build/failed_numbers.txt', 'a') as f:
                f.write(f'\n\n')
                for num in failed_numbers:
                    f.write(f'{num}\n')
            print(f'Failed numbers have been written to failed_numbers.txt')
        else:
            print('\nAll numbers passed successfully!')

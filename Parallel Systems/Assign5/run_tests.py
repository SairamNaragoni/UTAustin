#!/usr/bin/env python3

import os
import subprocess
from subprocess import check_output
import re
import pickle
from time import sleep

def make_dir(dir_name):
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)

def save_data(data, file_path):
    OP_FILE_DIR = "output"
    make_dir("output")
    with open(f'{OP_FILE_DIR}/{file_path}.pkl', 'wb') as f:
        pickle.dump(data, f)


def extract_timings(output, input_file, steps, np, mpi):
    # Define the dictionary to store all timings
    timings = {
        "total_time": None,
        "avg_step_time": None,
        "build_tree_time": None,
        "compute_forces_time": None,
        "update_position_velocity_time": None
    }

    # Define regular expressions to match each timing value
    timing_patterns = {
        "total_time": r'total_time:(\d+)',
        "avg_step_time": r'avg_step_time:(\d+)',
        "build_tree_time": r'build_tree_time:(\d+)',
        "compute_forces_time": r'compute_forces_time:(\d+)',
        "update_position_velocity_time": r'update_position_velocity_time:(\d+)'
    }

    # Search for each timing value in the output and store it in the dictionary
    for key, pattern in timing_patterns.items():
        match = re.search(pattern, output)
        if match:
            timings[key] = match.group(1)  # Extract the time in ms
        else:
            print(f"{key} not found in the output.")

    # Print the extracted timings
    print(f"Extracted Timings for total_time={timings['total_time']}, file={input_file}, steps={steps}, np={np}, mpi={mpi}:")
    # for key, value in timings.items():
    #     print(f"{key}: {value}")

    print("*****************************************************************")

    return timings

def run_mpi_program(input_file, output_file, steps, np, mpi=False):
    # Define the command and its arguments with the passed parameters
    if mpi:
        command = [
            'mpirun',              # Run the MPI program
            '-np', str(np),        # Number of processes
            './nbody',         # Path to the compiled executable
            '-i', input_file,      # Input file path
            '-o', output_file,     # Output file path
            '-s', str(steps),      # Number of steps
            '-t', '0.5',           # theta
            '-d', '0.005'         # Delta
        ]
    else:
        command = [
            './nbody',         # Path to the compiled executable
            '-i', input_file,      # Input file path
            '-o', output_file,     # Output file path
            '-s', str(steps),      # Number of steps
            '-t', '0.5',           # theta
            '-d', '0.005'         # Delta   
            '-S'     
            ]
    try:
        # Run the command and capture the output
        result = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        # Capture the output from stdout
        output = result.stdout
        return extract_timings(output, input_file, steps, np, mpi)

    except subprocess.CalledProcessError as e:
        print(f"Error executing MPI program: {e}")
    except FileNotFoundError as e:
        print(f"Error: {e.strerror}")

n_processors = [1,2,4,8,16]
steps_array = [100, 1000, 5000]
files = ["nb-10.txt", "nb-100.txt", "nb-100000.txt"]

# steps_array = [10]
# n_processors = [2]

def generate_sequential_data():
    all_results = []
    for file in files:
        input_file_path = f"input/{file}"
        output_file_path = f"output/{file}"
        for steps in steps_array:
            output_fields = run_mpi_program(input_file_path, output_file_path, steps=steps, np=1, mpi=False)
            output_fields['file'] = file
            output_fields['steps'] = steps
            output_fields['n_processors'] = 1
            all_results.append(output_fields)
        print("------------------------------FILE COMPLETE------------------------------")
    save_data(all_results, "sequential_data")

def generate_mpi_data():
    all_results = []
    for file in files:
        input_file_path = f"input/{file}"
        output_file_path = f"output/mpi_{file}"
        for steps in steps_array:
            for np in n_processors:
                output_fields = run_mpi_program(input_file_path, output_file_path, steps=steps, np=np, mpi=True)
                output_fields['file'] = file
                output_fields['steps'] = steps
                output_fields['n_processors'] = np
                all_results.append(output_fields)
        print("------------------------------FILE COMPLETE------------------------------")
    save_data(all_results, "mpi_data")

# generate_sequential_data()
generate_mpi_data()


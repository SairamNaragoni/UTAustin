#!/usr/bin/env python3
import os
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


def extract_variables(output):
    variables = {
        'cuda_per_iteration': r'cuda_per_iteration:\s+([\d.]+)\s+ms',
        'cuda_elapsed_time': r'cuda_elapsed_time:\s+([\d.]+)\s+ms',
        'cuda_io_time': r'cuda_io_time:\s+([\d.]+)\s+ms',
        'cuda_percent_spent_in_io': r'cuda_percent_spent_in_io:\s+([\d.]+)',
        'thrust_per_iteration': r'thrust_per_iteration:\s+([\d.]+)\s+ms',
        'thrust_elapsed_time': r'thrust_elapsed_time:\s+([\d.]+)\s+ms',
        'thrust_io_time': r'thrust_io_time:\s+([\d.]+)\s+ms',
        'thrust_percent_spent_in_io': r'thrust_percent_spent_in_io:\s+([\d.]+)',
        'main_per_iteration': r'main_per_iteration:\s+([\d.]+)\s+ms',
        'main_total': r'main_total:\s+([\d.]+)\s+ms'
    }
    
    extracted_data = {}
    
    for var_name, regex in variables.items():
        match = re.search(regex, output)
        if match:
            if 'cuda_' in var_name:
                var_name = var_name.replace('cuda_', '')
            if 'thrust_' in var_name:
                var_name = var_name.replace('thrust_', '')
            extracted_data[var_name] = float(match.group(1))
    
    return extracted_data


def default(trial_id=0, alternate=False):
    FILES = {
        "random-n2048-d16-c16" : {
            "num_clusters": 16,
            "dims" : 16,
        },
        "random-n16384-d24-c16" : {
            "num_clusters": 16,
            "dims" : 24,
        },
        "random-n65536-d32-c16" : {
            "num_clusters": 16,
            "dims" : 32,
        }
    }
    seed = 8675309
    threshold = 0.000001
    max_iters = 150
    algorithms = {
        0 : 'sequential',
        1 : 'cuda',
        2 : 'cuda_shared_mem',
        3 : 'thrust',
    }
    threads = [512, 1024]

    all_results = []

    for file_name, values in FILES.items():
        num_clusters = values['num_clusters']
        dims = values['dims']
        print(f"*********** using file {file_name} **************")
        for algorithm, algo_name in algorithms.items():
            for num_threads in threads:
                print(f"executing {algo_name} with threads={num_threads}")
                if alternate:
                    cmd = f"./bin/kmeans -k {num_clusters} -d {dims} -i input/{file_name}.txt -m {max_iters} -s {seed} -t {threshold} -a {algorithm} -c -h {num_threads} -f"
                else:
                    cmd = f"./bin/kmeans -k {num_clusters} -d {dims} -i input/{file_name}.txt -m {max_iters} -s {seed} -t {threshold} -a {algorithm} -c -h {num_threads}"
                out = check_output(cmd, shell=True, start_new_session=True).decode("ascii")
                variables = extract_variables(out)
                variables['algo_name'] = algo_name
                variables['file_name'] = file_name
                variables['num_threads'] = num_threads
                variables['trial_id'] = trial_id
                print(variables)
                all_results.append(variables)
                sleep(0.5)

    return all_results

def save_results(num_trials = 3, alternate=False):
    for trial_id in range(0, num_trials):
        all_results = default(trial_id, alternate=alternate)
        if alternate:
            file_name = f"all_results_f_{trial_id}"
        else:
            file_name = f"all_results_{trial_id}"
        save_data(all_results, file_name)

save_results(3)
save_results(3, True)
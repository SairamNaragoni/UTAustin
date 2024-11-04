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

def extract_variables(output):
    print("Program Output:\n", output)
    # Parse the output to extract hashTime, hashGroupTime, and compareTreeTime
    hash_time = re.search(r'hashTime: (\d+\.\d+)', output)
    hash_group_time = re.search(r'hashGroupTime: (\d+\.\d+)', output)
    compare_tree_time = re.search(r'compareTreeTime: (\d+\.\d+)', output)
    extracted_data = {}
    
    if hash_time:
        extracted_data['hashTime'] = float(hash_time.group(1))
    if hash_group_time:
        extracted_data['hashGroupTime'] = float(hash_group_time.group(1))
    if compare_tree_time:
        extracted_data['compareTreeTime'] = float(compare_tree_time.group(1))

    return extracted_data

def run_go_program(go_file="src/BST_opt.go", input_file="input/simple.txt", hash_workers=1, data_workers=0, comp_workers=0, use_buffer=True, use_channels=False, use_workers=True):
    # Define the command and arguments
    cmd = [
        'go', 'run', go_file,               
        '-input', input_file,               
        '--hash-workers', str(hash_workers), 
        '--data-workers', str(data_workers),
        '--comp-workers', str(comp_workers), 
        f'--use-buffer={str(use_buffer).lower()}',
        f'--use-channels={str(use_channels).lower()}',
        f'--use-workers={str(use_workers).lower()}'
    ]

    try:
        output_fields = {}
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        output = result.stdout
        output_fields = extract_variables(output)
    except subprocess.CalledProcessError as e:
        print(f"Error: {e.stderr}")

    return output_fields


def run(approach, file, trial, hash_workers=1, data_workers=0, comp_workers=0, use_buffer=True, use_channels=False, use_workers=True):
    #sequential_results
    print(f"Running {approach} experiment, trial={trial}, file={file}")
    output_fields = run_go_program(
        input_file=f'input/{file}.txt',
        hash_workers=hash_workers,
        data_workers=data_workers,
        comp_workers=comp_workers,
        use_buffer=use_buffer,
        use_channels=use_channels,
        use_workers=use_workers
    )
    output_fields['hashWorkers'] = hash_workers
    output_fields['dataWorkers'] = data_workers
    output_fields['compWorkers'] = comp_workers
    output_fields['useBuffer'] = use_buffer
    output_fields['useChannels'] = use_channels
    output_fields['input_file'] = file
    output_fields['trial_id'] = trial
    output_fields['approach'] = approach
    return output_fields

def generate_data(approach, hash_workers, data_workers, comp_workers, use_buffer, use_channels, use_workers, worker_trials = False):
    all_results = []
    for file, value in files.items():
        n_trees = value['n_trees']
        hash_workers_trials_l = list(hash_worker_trials)
        for trial in range(num_trials):
            if not worker_trials:
                output_fields = run(approach, file, trial, hash_workers, data_workers, comp_workers, use_buffer, use_channels, use_workers)
                print(output_fields)
                all_results.append(output_fields)
            else:
                for hash_worker_num in hash_workers_trials_l:
                    output_fields = run(approach, file, trial, hash_worker_num, data_workers, comp_workers, use_buffer, use_channels, use_workers)
                    print(output_fields)
                    all_results.append(output_fields)

    save_data(all_results, f"{approach}")


def generate_hashgroups_data(approach, use_buffer, use_channels):
    all_results = []
    for file, value in files.items():
        n_trees = value['n_trees']
        hash_workers_trials_l = list(hash_worker_trials)
        hash_workers_trials_l.append(n_trees)
        for trial in range(num_trials):
            for hash_worker_num in hash_workers_trials_l:
                for data_worker_num in data_workers_trials:
                    if data_worker_num <= hash_worker_num:
                        algo_approach = approach
                        if data_worker_num == 1:
                            algo_approach = 'channel'
                        elif data_worker_num == hash_worker_num:
                            algo_approach = 'single_mutexes'
                        else:
                            if use_channels:
                                algo_approach = 'multiple_mutexes_channel'
                            else:
                                algo_approach = 'multiple_mutexes'
                        output_fields = run(algo_approach, file, trial, hash_worker_num, data_worker_num, 0, use_buffer, use_channels, True)
                        print(output_fields)
                        all_results.append(output_fields)
    save_data(all_results, f"{approach}")

def generate_comp_data(approach, use_buffer, use_channels):
    all_results = []
    for file, value in files.items():
        n_trees = value['n_trees']
        hash_workers_trials_l = list(hash_worker_trials)
        hash_workers_trials_l.append(n_trees)
        for trial in range(num_trials):
            for hash_worker_num in hash_workers_trials_l:
                for data_worker_num in data_workers_trials:
                    if data_worker_num <= hash_worker_num:
                        for comp_worker_num in comp_workers_trails:
                            algo_approach = approach
                            if data_worker_num == 1:
                                algo_approach = 'channel'
                            elif data_worker_num == hash_worker_num:
                                algo_approach = 'single_mutexes'
                            else:
                                if use_channels:
                                    algo_approach = 'multiple_mutexes_channel'
                                else:
                                    algo_approach = 'multiple_mutexes'
                            output_fields = run(algo_approach, file, trial, hash_worker_num, data_worker_num, comp_worker_num, use_buffer, use_channels, True)
                            print(output_fields)
                            all_results.append(output_fields)
    save_data(all_results, f"{approach}")

#Global Vars
files = {
    "fine" : {
        "n_trees": 100000
    },
    "coarse" : {
        "n_trees": 100
    }
}

# hash_worker_trials = [2,4,8,16]
data_workers_trials = [1,2,4,8,16]
comp_workers_trails = [1,2,4,8,16]
num_trials = 10

generate_data("sequential", 1, 1, 1, False, False, False)
generate_data("hashing_per_tree", 2, 0, 0, False, False, False, False)
generate_data("hashing_per_workers", 2, 0, 0, False, False, True, True)

#hashworkers 4 appear to be fastest so far for fine
generate_hashgroups_data("hashgroups_channel", False, True)
generate_hashgroups_data("hashgroups_mutexes", False, False)


#Takes 2+ hours to finish for all trials
hash_worker_trials = [4,16]
# generate_comp_data("comp_groups_buffer_channel", True, True)
generate_comp_data("comp_groups_buffer_mutexes", True, False)
# generate_comp_data("comp_groups_unbuffer_channel", False, True)
generate_comp_data("comp_groups_unbuffer_mutexes", False, False)
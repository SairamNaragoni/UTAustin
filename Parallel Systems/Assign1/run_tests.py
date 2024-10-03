#!/usr/bin/env python3
import os
from subprocess import check_output
import re
from time import sleep
import pickle

#
#  Feel free (a.k.a. you have to) to modify this to instrument your code
#
def make_dir(dir_name):
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)

def save_data(data, file_path):
    OP_FILE_DIR = "output"
    make_dir("output")
    with open(f'{OP_FILE_DIR}/{file_path}.pkl', 'wb') as f:
        pickle.dump(data, f)

def question_one():
    THREADS = [i for i in range(0, 34, 2)]
    LOOPS = [100000]
    INPUTS = ["1k.txt", "8k.txt", "16k.txt"]
    csvs = default(THREADS, LOOPS, INPUTS)
    save_data(csvs, "q1")

def question_two():
    THREADS = [i for i in range(0, 34, 2)]
    LOOPS = [10]
    INPUTS = ["1k.txt", "8k.txt", "16k.txt"]
    csvs = default(THREADS, LOOPS, INPUTS)
    save_data(csvs, "q2")

def find_inflexion(spin=False):
    THREADS = [i for i in range(0, 16, 2)]
    LOOPS = [10, 100, 200, 500, 1000, 3000, 6000, 10000]
    INPUTS = ["1k.txt", "8k.txt", "16k.txt"]
    csvs = default(THREADS, LOOPS, INPUTS, spin)
    file_name = "inx_spin" if spin else "inx"
    save_data(csvs, file_name)

def question_three():
    THREADS = [i for i in range(0, 34, 2)]
    LOOPS = [10, 100000]
    INPUTS = ["1k.txt", "8k.txt", "16k.txt"]
    csvs = default(THREADS, LOOPS, INPUTS, True)
    save_data(csvs, "q3")

def default(THREADS=[2], LOOPS=[1], INPUTS=["seq_64_test.txt"], spin=False):
    csvs = []
    for inp in INPUTS:
        for loop in LOOPS:
            for thr in THREADS:
                if spin:
                    cmd = "./bin/prefix_scan -o temp.txt -n {} -i tests/{} -l {} -s".format(thr, inp, loop)
                else:
                    cmd = "./bin/prefix_scan -o temp.txt -n {} -i tests/{} -l {}".format(thr, inp, loop)
                out = check_output(cmd, shell=True).decode("ascii")
                m = re.search("time: (.*)", out)
                if m is not None:
                    time = m.group(1)
                csv =  {
                    'file' : inp,
                    'loop' : loop,
                    'thread': thr,
                    'time' : time,
                }
                csvs.append(csv)
            sleep(0.5)

    print("\n")
    for csv in csvs:
        print(csv)
    
    return csvs

default()
question_one()
question_two()
find_inflexion()
question_three()
find_inflexion(True)
import os
import json
import argparse
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import csv
from matplotlib import cm

matplotlib.rc('font', size=14)

def read_descriptor_from_json(descriptor_filename):
    # Read the descriptor data from a JSON file
    try:
        with open(descriptor_filename, 'r') as json_file:
            descriptor_data = json.load(json_file)
        return descriptor_data
    except FileNotFoundError:
        print(f"Error: File '{descriptor_filename}' not found.")
        return None
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON in file '{descriptor_filename}': {e}")
        return None

def get_IPC(descriptor_data, sim_path, output_dir):
  benchmarks_org = descriptor_data["workloads_list"].copy()
  benchmarks = []
  ipc = {}

  try:
    for config_key in descriptor_data["configurations"].keys():
      ipc_config = []
      avg_IPC_config = 0.0
      cnt_benchmarks = 0
      for benchmark in benchmarks_org:
        benchmark_name = benchmark.split("/")
        exp_path = sim_path+'/'+benchmark+'/'+descriptor_data["experiment"]+'/'
        IPC = 0
        with open(exp_path+config_key+'/memory.stat.0.csv') as f:
          lines = f.readlines()
          for line in lines:
            if 'Periodic IPC' in line:
              tokens = [x.strip() for x in line.split(',')]
              IPC = float(tokens[1])
              break

        avg_IPC_config += IPC

        cnt_benchmarks = cnt_benchmarks + 1
        if len(benchmarks_org) > len(benchmarks):
          benchmarks.append(benchmark_name)

        ipc_config.append(IPC)

      num = len(benchmarks)
      print(benchmarks)
      ipc_config.append(avg_IPC_config/num)
      ipc[config_key] = ipc_config

    benchmarks.append('Avg')
    plot_data(benchmarks, ipc, 'IPC', output_dir+'/FigureA.png')

  except Exception as e:
    print(e)

def get_dcache_miss_ratio(descriptor_data, sim_path, output_dir):
    benchmarks_org = descriptor_data["workloads_list"].copy()
    benchmarks = []
    dcache_miss_ratio = {}

    try:
        for config_key in descriptor_data["configurations"].keys():
            dcache_miss_config = []
            avg_dcache_miss_ratio = 0.0
            cnt_benchmarks = 0

            for benchmark in benchmarks_org:
                benchmark_name = benchmark.split("/")
                exp_path = sim_path + '/' + benchmark + '/' + descriptor_data["experiment"] + '/'
                dcache_misses = 0
                dcache_hits = 0
                dcache_miss_ratio_val = 0.0

                with open(exp_path + config_key + '/memory.stat.0.csv') as f:
                    lines = f.readlines()
                    for line in lines:
                        if 'DCACHE_MISS_count' in line and 'NUM_WINDOWS_WITH_DCACHE_MISS_count' not in line:
                            tokens = [x.strip() for x in line.split(',')]
                            dcache_misses = float(tokens[1])
                        elif 'DCACHE_HIT_count' in line and 'DC_PREF_REQ_DCACHE_HIT_count' not in line:
                            tokens = [x.strip() for x in line.split(',')]
                            dcache_hits = float(tokens[1])

                # Calculate Dcache miss ratio after reading all lines
                if dcache_hits + dcache_misses > 0:
                    dcache_miss_ratio_val = dcache_misses / (dcache_hits + dcache_misses)
                else:
                    dcache_miss_ratio_val = 0.0  # Handle edge case where both are 0

                avg_dcache_miss_ratio += dcache_miss_ratio_val
                cnt_benchmarks += 1

                if len(benchmarks_org) > len(benchmarks):
                    benchmarks.append(benchmark_name)

                dcache_miss_config.append(dcache_miss_ratio_val)
                print(f"Benchmark: {benchmark_name}, Misses: {dcache_misses}, Hits: {dcache_hits}, Miss Ratio: {dcache_miss_ratio_val}")

            # Calculate average after all benchmarks
            if cnt_benchmarks > 0:
                avg_dcache_miss_ratio /= cnt_benchmarks
                dcache_miss_config.append(avg_dcache_miss_ratio)
                dcache_miss_ratio[config_key] = dcache_miss_config

        benchmarks.append('Avg')
        print("Dcache Miss Ratio Data:", dcache_miss_ratio)
        plot_data(benchmarks, dcache_miss_ratio, 'Dcache Miss Ratio', output_dir + '/FigureB.png')

    except Exception as e:
        print(f"Error occurred: {e}")
        
def get_umlc_hit_ratio(descriptor_data, sim_path, output_dir):
    benchmarks_org = descriptor_data["workloads_list"].copy()
    benchmarks = []
    umlc_hit_ratio = {}

    try:
        for config_key in descriptor_data["configurations"].keys():
            umlc_hit_config = []
            avg_umlc_hit_ratio = 0.0
            cnt_benchmarks = 0

            for benchmark in benchmarks_org:
                benchmark_name = benchmark.split("/")
                exp_path = sim_path + '/' + benchmark + '/' + descriptor_data["experiment"] + '/'
                umlc_misses = 0
                umlc_hits = 0
                umlc_hit_ratio_val = 0.0

                with open(exp_path + config_key + '/memory.stat.0.csv') as f:
                    lines = f.readlines()
                    for line in lines:
                        if 'MLC_MISS_count' in line and 'CORE_MLC_MISS_count' not in line:
                            tokens = [x.strip() for x in line.split(',')]
                            umlc_misses = float(tokens[1])
                        elif 'MLC_HIT_count' in line and \
                        'MLC_NEWREQ_MATCHED_L2_PREF_MLC_HIT_count' not in line and \
                        'CORE_MLC_HIT_count' not in line:
                            tokens = [x.strip() for x in line.split(',')]
                            umlc_hits = float(tokens[1])

                # Calculate UMLC miss ratio after reading all lines
                if umlc_hits + umlc_misses > 0:
                    umlc_hit_ratio_val = umlc_hits / (umlc_hits + umlc_misses)
                else:
                    umlc_hit_ratio_val = 0.0  # Handle edge case where both are 0

                avg_umlc_hit_ratio += umlc_hit_ratio_val
                cnt_benchmarks += 1

                if len(benchmarks_org) > len(benchmarks):
                    benchmarks.append(benchmark_name)

                umlc_hit_config.append(umlc_hit_ratio_val)
                print(f"Benchmark: {benchmark_name}, Misses: {umlc_misses}, Hits: {umlc_hits}, Hit Ratio: {umlc_hit_ratio_val}")

            # Calculate average after all benchmarks
            if cnt_benchmarks > 0:
                avg_umlc_hit_ratio /= cnt_benchmarks
                umlc_hit_config.append(avg_umlc_hit_ratio)
                umlc_hit_ratio[config_key] = umlc_hit_config

        benchmarks.append('Avg')
        print("UMLC Hit Ratio Data:", umlc_hit_ratio)
        plot_data(benchmarks, umlc_hit_ratio, 'UMLC Hit Ratio', output_dir + '/FigureC.png')

    except Exception as e:
        print(f"Error occurred: {e}")
        
def get_umlc_miss_count(descriptor_data, sim_path, output_dir):
  benchmarks_org = descriptor_data["workloads_list"].copy()
  benchmarks = []
  umlc_miss_count = {}

  try:
    for config_key in descriptor_data["configurations"].keys():
      umlc_miss_count_config = []
      avg_umlc_miss_count_config = 0.0
      cnt_benchmarks = 0
      for benchmark in benchmarks_org:
        benchmark_name = benchmark.split("/")
        exp_path = sim_path+'/'+benchmark+'/'+descriptor_data["experiment"]+'/'
        MLC_MISSES = 0
        with open(exp_path+config_key+'/memory.stat.0.csv') as f:
          lines = f.readlines()
          for line in lines:
            if 'MLC_MISS_count' in line and 'CORE_MLC_MISS_count' not in line:
                tokens = [x.strip() for x in line.split(',')]
                MLC_MISSES = float(tokens[1])

        avg_umlc_miss_count_config += MLC_MISSES

        cnt_benchmarks = cnt_benchmarks + 1
        if len(benchmarks_org) > len(benchmarks):
          benchmarks.append(benchmark_name)

        umlc_miss_count_config.append(MLC_MISSES)

      num = len(benchmarks)
      print(benchmarks)
      umlc_miss_count_config.append(avg_umlc_miss_count_config/num)
      umlc_miss_count[config_key] = umlc_miss_count_config

    benchmarks.append('Avg')
    plot_data(benchmarks, umlc_miss_count, 'L2 Cache Miss Counts', output_dir+'/FigureD.png')
  except Exception as e:
    print(e)

def get_L3_miss_count(descriptor_data, sim_path, output_dir):
  benchmarks_org = descriptor_data["workloads_list"].copy()
  benchmarks = []
  L3_miss_count = {}

  try:
    for config_key in descriptor_data["configurations"].keys():
      L3_miss_count_config = []
      avg_L3_miss_count_config = 0.0
      cnt_benchmarks = 0
      for benchmark in benchmarks_org:
        benchmark_name = benchmark.split("/")
        exp_path = sim_path+'/'+benchmark+'/'+descriptor_data["experiment"]+'/'
        L3_MISSES = 0
        with open(exp_path+config_key+'/memory.stat.0.csv') as f:
          lines = f.readlines()
          for line in lines:
            if 'L1_MISS_count' in line and 'DC_PREF_REQ_L1_MISS_count' not in line \
            and 'DC_PREF_HIT_L1_MISS_count' not in line and 'CORE_L1_MISS_count' not in line:
                tokens = [x.strip() for x in line.split(',')]
                L3_MISSES = float(tokens[1])

        avg_L3_miss_count_config += L3_MISSES

        cnt_benchmarks = cnt_benchmarks + 1
        if len(benchmarks_org) > len(benchmarks):
          benchmarks.append(benchmark_name)

        L3_miss_count_config.append(L3_MISSES)

      num = len(benchmarks)
      print(benchmarks)
      L3_miss_count_config.append(avg_L3_miss_count_config/num)
      L3_miss_count[config_key] = L3_miss_count_config

    benchmarks.append('Avg')
    plot_data(benchmarks, L3_miss_count, 'L3 Cache Miss Counts', output_dir+'/FigureE.png')
  except Exception as e:
    print(e)
    
def plot_data(benchmarks, data, ylabel_name, fig_name, ylim=None):
    colors = [
        '#1f77b4',  # Blue
        '#ff7f0e',  # Orange
        '#2ca02c',  # Green
        '#d62728',  # Red
        '#9467bd',  # Purple
        '#8c564b',  # Brown
        '#e377c2',  # Pink
        '#7f7f7f',  # Gray
        '#bcbd22',  # Yellow-green
        '#17becf',  # Cyan
    ]

    num_keys = len(data.keys())
    ind = np.arange(len(benchmarks))  # Define ind based on benchmarks length
    width = 0.10  # Define the width of the bars

    num_plots = (len(benchmarks) + 7) // 8  # Number of plots needed
    for plot_idx in range(num_plots):
        start_idx = plot_idx * 8
        end_idx = min(start_idx + 8, len(benchmarks))
        
        fig, ax = plt.subplots(figsize=(14, 4.4), dpi=80)
        
        for idx, key in enumerate(data.keys()):
            ax.bar(ind[start_idx:end_idx] + idx * width, data[key][start_idx:end_idx], width=width, label=key)

        ax.set_xlabel("Benchmarks")
        ax.set_ylabel(ylabel_name)
        ax.set_xticks(ind[start_idx:end_idx] + (num_keys / 2 - 0.5) * width)
        ax.set_xticklabels(benchmarks[start_idx:end_idx], rotation=27, ha='right')
        ax.legend(loc="upper left", ncols=2)

        if ylim is not None:
            ax.set_ylim(ylim)
        
        fig.tight_layout()
        plt.savefig(fig_name.replace('.png', f'_{plot_idx}.png'), format="png", bbox_inches="tight")

def get_dcache_miss_statistics(descriptor_data, sim_path, output_dir):
    benchmarks, miss_types = get_dcache_miss_types(descriptor_data, sim_path)
    print("Dcache Miss Type Data:", miss_types)
    plot_stacked_bar(benchmarks, miss_types, 'Dcache Miss Types', output_dir + '/StackedDcacheMissTypes.png')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Read descriptor file name')
    parser.add_argument('-o','--output_dir', required=True, help='Output path. Usage: -o /home/$USER/plot')
    parser.add_argument('-d','--descriptor_name', required=True, help='Experiment descriptor name. Usage: -d /home/$USER/lab1.json')
    parser.add_argument('-s','--simulation_path', required=True, help='Simulation result path. Usage: -s /home/$USER/exp/simulations')

    args = parser.parse_args()
    descriptor_filename = args.descriptor_name

    descriptor_data = read_descriptor_from_json(descriptor_filename)
    get_IPC(descriptor_data, args.simulation_path, args.output_dir)
    get_dcache_miss_ratio(descriptor_data, args.simulation_path, args.output_dir)
    get_umlc_hit_ratio(descriptor_data, args.simulation_path, args.output_dir)
    get_umlc_miss_count(descriptor_data, args.simulation_path, args.output_dir)
    get_L3_miss_count(descriptor_data, args.simulation_path, args.output_dir)
    
    plt.grid('x')
    plt.tight_layout()
    plt.show()

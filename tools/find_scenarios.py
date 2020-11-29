"""
A simple module for finding scenarios in well known locations
"""
import re
import os

SOURCE_DIR = os.path.join(os.path.dirname(__file__), '..')

scenario_locations = [
    os.path.join(SOURCE_DIR, 'src', 'libtpm2', 'evaluate_solvers', 'scenarios')
]

def find_scenarios():
    """
    :returns list(str, str): A list of pairs (name, path)
    """
    scenarios = []

    for location in scenario_locations:
        for _file in os.listdir(location):
            if _file.startswith('.') or not _file.endswith('.xml'):
                continue

            if os.path.isfile(os.path.join(location, _file)):
                scenarios.append((_file[:-4], os.path.join(location, _file)))

    return scenarios

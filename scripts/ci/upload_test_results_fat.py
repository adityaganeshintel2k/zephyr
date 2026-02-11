#!/usr/bin/env python3

# Copyright (c) 2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# This script upload fat test ci results to the zephyr ES instance for reporting and analysis.
# see https://kibana.zephyrproject.io/


from elasticsearch import Elasticsearch
from elasticsearch.helpers import bulk
import os
import json
import argparse
from datetime import datetime
from pathlib import Path

def load_json(file_path):
    with open(file_path, 'r') as file:
        return json.load(file)

def append_json(existing_data, json_file_path):
    additional_data = load_json(json_file_path)
    existing_data["tests"].extend(additional_data)

def find_json_files(directory_path):
    return list(Path(directory_path).rglob("*test_result.json"))

def gendata(data, index, run_date=None):
    for test in data['tests']:
        test['run_date'] = run_date
        yield {
            "_index": index,
            "_source": test
        }

def create_elasticsearch_instance():
    return Elasticsearch(
        [os.environ['ELASTICSEARCH_SERVER']],
        api_key=os.environ['ELASTICSEARCH_KEY'],
        verify_certs=False
    )

def create_index(es, index_name):
    settings = {
        "index": {
            "number_of_shards": 4
        }
    }
    mappings = {}
    es.indices.create(index=index_name, mappings=mappings, settings=settings)

def parse_args():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('-y', '--dry-run', action="store_true", help='Dry run.')
    parser.add_argument('-c', '--create-index', action="store_true", help='Create index.')
    parser.add_argument('-i', '--index', help='Index to push to.', required=True)
    parser.add_argument('-r', '--run-date', help='Run date in ISO format.')
    parser.add_argument('folder', help='Folder with test data.')
    return parser.parse_args()

def main():

    args = parse_args()
    selected_path = args.folder
    if not Path(selected_path).exists():
        print(f"The path '{selected_path}' does not exist.")
        return

    combined_test_data = {"tests": []}
    for json_file in find_json_files(selected_path):
        append_json(combined_test_data, json_file)

    es = create_elasticsearch_instance()

    if args.create_index:
        create_index(es, args.index)

    if not args.run_date:
        time = os.path.getmtime(selected_path)
        args.run_date = datetime.fromtimestamp(time).isoformat()

    if not args.dry_run:
        bulk(es, gendata(combined_test_data, args.index, args.run_date))

if __name__ == '__main__':
    main()

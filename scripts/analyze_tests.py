#!/usr/bin/env python3
"""Analyze test files to find overlaps, issues, and actual vs expected behavior."""

import os
import re
from collections import defaultdict

def analyze_test_file(filepath):
    """Extract key information from a test file."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Extract test functions
    test_functions = re.findall(r'void\s+(test_\w+)\s*\(', content)
    
    # Look for assertions
    assertions = re.findall(r'assert\((.*?)\)', content, re.DOTALL)
    
    # Look for what's being tested
    includes = re.findall(r'#include\s+[<"](.*?)[>"]', content)
    
    # Look for key operations
    operations = {
        'planet_creation': len(re.findall(r'OctreePlanet\s+\w+\s*\(', content)),
        'generate_calls': len(re.findall(r'\.generate\(', content)),
        'prepareRenderData': len(re.findall(r'prepareRenderData\(', content)),
        'material_checks': len(re.findall(r'getDominantMaterial\(', content)),
        'voxel_checks': len(re.findall(r'voxels\[', content)),
        'gpu_related': len(re.findall(r'GPU|gpu', content)),
    }
    
    # Check if test is actually compiled/run
    basename = os.path.basename(filepath).replace('.cpp', '')
    
    return {
        'file': basename,
        'functions': test_functions,
        'assertion_count': len(assertions),
        'includes': includes,
        'operations': operations,
        'line_count': len(content.split('\n'))
    }

def main():
    test_dir = 'tests'
    tests = []
    
    for filename in os.listdir(test_dir):
        if filename.endswith('.cpp'):
            filepath = os.path.join(test_dir, filename)
            tests.append(analyze_test_file(filepath))
    
    # Sort by name
    tests.sort(key=lambda x: x['file'])
    
    print("=" * 80)
    print("TEST SUITE ANALYSIS")
    print("=" * 80)
    print(f"\nTotal test files: {len(tests)}")
    
    # Find overlapping tests (similar operations)
    print("\n## POTENTIALLY OVERLAPPING TESTS (similar operation patterns):")
    print("-" * 40)
    
    # Group by operation similarity
    groups = defaultdict(list)
    for test in tests:
        ops = test['operations']
        # Create a signature based on operations
        sig = f"planet:{ops['planet_creation']}_render:{ops['prepareRenderData']}_material:{ops['material_checks']}"
        groups[sig].append(test['file'])
    
    for sig, files in groups.items():
        if len(files) > 1:
            print(f"\n{sig}")
            for f in files:
                print(f"  - {f}")
    
    # Find tests with no assertions
    print("\n## TESTS WITH FEW/NO ASSERTIONS (might not test anything):")
    print("-" * 40)
    for test in tests:
        if test['assertion_count'] < 2:
            print(f"  {test['file']}: {test['assertion_count']} assertions")
    
    # Find GPU-related tests
    print("\n## GPU-RELATED TESTS:")
    print("-" * 40)
    gpu_tests = [t for t in tests if t['operations']['gpu_related'] > 0]
    for test in gpu_tests:
        print(f"  {test['file']}: {test['operations']['gpu_related']} GPU references")
    
    # Find material-related tests
    print("\n## MATERIAL-FOCUSED TESTS:")
    print("-" * 40)
    material_tests = [t for t in tests if 'material' in test['file'].lower() or test['operations']['material_checks'] > 5]
    for test in material_tests:
        print(f"  {test['file']}: {test['operations']['material_checks']} material checks")
    
    # Show test complexity
    print("\n## TEST COMPLEXITY (by line count):")
    print("-" * 40)
    tests_by_size = sorted(tests, key=lambda x: x['line_count'], reverse=True)[:10]
    for test in tests_by_size:
        print(f"  {test['file']}: {test['line_count']} lines, {len(test['functions'])} test functions")
    
    # Summary statistics
    total_assertions = sum(t['assertion_count'] for t in tests)
    total_functions = sum(len(t['functions']) for t in tests)
    total_lines = sum(t['line_count'] for t in tests)
    
    print("\n## SUMMARY STATISTICS:")
    print("-" * 40)
    print(f"  Total test functions: {total_functions}")
    print(f"  Total assertions: {total_assertions}")
    print(f"  Total lines of test code: {total_lines}")
    print(f"  Average assertions per test file: {total_assertions/len(tests):.1f}")
    print(f"  Average lines per test file: {total_lines/len(tests):.1f}")

if __name__ == "__main__":
    main()
#!/usr/bin/env python3

import os
import sys
import subprocess
import hashlib
import pickle

if 'BUILD_TEMP' not in os.environ:
    sys.stderr.write("BUILD_TEMP must be set")
    sys.exit(-1)

BUILD_TEMP = os.environ['BUILD_TEMP']
SLASH = bytes(os.path.sep, 'utf8')

def call(*cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    for line in proc.stdout:
        yield line

def find_all(path, suffix):
    for root, dirs, files in os.walk(path):
        for name in files:
            if name.endswith('.mk'):
                yield os.path.join(root, name)

def list_all_packages():
    return [bytes(B, 'utf8') for B in os.listdir('package')]

def hash_package_makefiles():
    h = hashlib.new('SHA256')
    for mk in find_all('package', '.mk'):
        with open(mk, 'rb') as fp:
            h.update(fp.read())
    h.update(bytes.join(b'', list_all_packages()))
    return h.hexdigest()

def load_cache(hash_digest):
    cached_deps = os.path.join(BUILD_TEMP, 'piksi_buildroot_deps.' + hash_digest)
    if not os.path.exists(cached_deps):
        return (None, cached_deps)
    try:
        with open(cached_deps, 'rb') as fp:
            return (pickle.load(fp), cached_deps)
    except:
        return (None, cached_deps)
            
def scrape_deps():
    hash_digest = hash_package_makefiles()
    cache, cache_path = load_cache(hash_digest)
    if cache is not None:
        return cache
    sys.stderr.write('Building cache of dependency information...\n')
    cache = {}
    for package in list_all_packages():
        cache[package] = []
    for line in call('make', '_print_db'):
        if line.lstrip().startswith(b'#'):
            continue
        if b'=' not in line:
            continue
        if b'_DEPENDENCIES' not in line:
            continue
        entry = line.split(b'=', 1)
        name = entry[0].strip().rsplit(b'_', 1)[0].lower()
        value = entry[1].strip().split()
        ls = cache.get(name, [])
        ls.extend(value)
    with open(cache_path, 'wb') as fp:
        pickle.dump(cache, fp)
    return cache

def topological(graph):
    # https://gist.github.com/kachayev/5910538
    from collections import deque
    GRAY, BLACK = 0, 1
    order, enter, state = deque(), set(graph), {}
    def dfs(node):
        state[node] = GRAY
        for k in graph.get(node, ()):
            sk = state.get(k, None)
            if sk == GRAY: raise ValueError('cycle')
            if sk == BLACK: continue
            enter.discard(k)
            dfs(k)
        order.appendleft(node)
        state[node] = BLACK
    while enter: dfs(enter.pop())
    return order

packages = set()

def get_files_changed():
    if 'SINCE' in os.environ and os.environ['SINCE'] != '':
        since = os.environ['SINCE']
        for line in call('git', 'diff', '--name-only', since):
            yield line
    for line in call('git', 'status', '-s'):
        line = line[3:]
        yield line

for line in get_files_changed():
    if not line.startswith(b'package' + SLASH):
        continue
    package = line.split(b'package' + SLASH, 1)[1]
    package = package.split(SLASH)[0]
    packages.add(package)

deps = scrape_deps()

for dep in reversed(topological(deps)):
    if dep in packages:
        target = (dep + b'-rebuild').decode('utf8')
        print(target)

# vim: et:sts=4:sw=4:ts=4:

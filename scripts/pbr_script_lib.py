#!/usr/bin/env python

# Copyright (C) 2018 Swift Navigation Inc.
# Contact: Swift Navigation <dev@swiftnav.com>
#
# This source is subject to the license found in the file 'LICENSE' which must
# be be distributed together with this source. All other rights reserved.
#
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
# EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Script for kicking off HITL smoke tests on piksi_firmware_private Pull
# Requests. Expects HITL_API_GITHUB_TOKEN and GITHUB_COMMENT_TOKEN to be in
# environmental variables.

import os
import os.path as OP
import sys
import subprocess
try:
    import urlparse
except ImportError:
    import urllib.parse as urlparse
import urllib
import platform
from functools import wraps
import hashlib

PREFLIGHT_CONTEXT_FILE = '.preflight_context.pickle'
_PREFLIGHT_CONTEXT = None


class Runner(object):
    """
    Helper object for running command pipelines.
    """

    def __init__(self):
        self._cat_filename = None
        self._stdin_fp = None
        self._proc_args = None
        self._proc_kw = None

    def _fetch_and_clear(self, name):
        v = getattr(self, name)
        setattr(self, name, None)
        return v

    def _make_proc(self, **kw):
        proc_args = self._fetch_and_clear('_proc_args')
        proc_kw = self._fetch_and_clear('_proc_kw')
        proc_kw.update(kw)
        return subprocess.Popen(*proc_args, **proc_kw)

    def cat(self, filename):
        """
        Cat a file so that it can be piped to a command
        """
        self._cat_filename = filename
        return self

    def pipe(self):
        """
        Setup of a pipe from whatever is before the pipe.
        """
        if self._cat_filename is not None:
            self._stdin_fp = open(self._fetch_and_clear('_cat_filename'), 'r')
        elif self._proc_args is not None:
            self._stdin_fp = self._make_proc(stdout=subprocess.PIPE).stdout
        else:
            raise ValueError('Nothing staged for pipe()')
        return self

    def call(self, *cmd):
        """
        Run a shell command.
        """
        if self._stdin_fp is not None:
            self._proc_args = [cmd]
            self._proc_kw = { 'stdin': self._stdin_fp }
        else:
            self._proc_args = [cmd]
            self._proc_kw = {}
        return self

    def to_str(self):
        """
        Capture the stdout of a pipeline.
        """
        if self._stdin_fp is not None:
            self._proc_kw = { 'stdin': self._stdin_fp }
        proc = self._make_proc(stdout=subprocess.PIPE)
        return proc.stdout.read().decode('utf8').strip()

    def exit_code(self):
        """
        Run a pipeline and check that it's successful.
        """
        proc = self._make_proc(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ret = proc.wait()
        return (ret, proc.stdout.read().decode('utf8'), proc.stderr.read().decode('utf8'))

    def check(self, stdout=None):
        """
        Run a pipeline and check that it's successful.
        """
        if stdout is None:
            proc = self._make_proc(stderr=subprocess.PIPE)
        else:
            proc = self._make_proc(stdout=stdout, stderr=subprocess.PIPE)
        ret = proc.wait()
        if ret != 0:
            sys.stdout.write('\n')
            raise ValueError(
                "Process signaled error (exit code {}), standard error:\n\n{}"
                .format(ret, proc.stderr.read().decode('utf8')))


def find_dot_git():
    curdir = os.getcwd()
    try:
        while os.getcwd() != '/':
            if os.path.exists('.git') and os.path.isdir('.git'):
                return os.getcwd()
            os.chdir('..')
    finally:
        os.chdir(curdir)
    return None


# Adapted from https://stackoverflow.com/a/44552191/749342
def build_url(baseurl, path, query_args):
    NETLOC_INDEX, QUERY_INDEX = 2, 4
    url_parts = list(urlparse.urlparse(baseurl))
    url_parts[NETLOC_INDEX] = urlparse.urljoin(url_parts[NETLOC_INDEX], path)
    url_parts[QUERY_INDEX]= urllib.urlencode(query_args)
    return urlparse.urlunparse(url_parts)


def cd_to_git_root(func):
    @wraps(func)
    def wrapper(*args, **kwds):
        old_cwd = os.getcwd()
        try:
            return func(*args, **kwds)
        finally:
            os.chdir(old_cwd)
    return wrapper


@cd_to_git_root
def clear_preflight_context():
    if os.path.exists(PREFLIGHT_CONTEXT_FILE):
        os.unlink(PREFLIGHT_CONTEXT_FILE)


@cd_to_git_root
def load_preflight_context():
    global _PREFLIGHT_CONTEXT
    if not os.path.exists(PREFLIGHT_CONTEXT_FILE):
        _PREFLIGHT_CONTEXT = {}
        return
    import pickle
    _PREFLIGHT_CONTEXT = pickle.load(open(PREFLIGHT_CONTEXT_FILE, 'rb'))


def get_preflight_context():
    if _PREFLIGHT_CONTEXT is None:
        load_preflight_context()
        import atexit
        atexit.register(save_preflight_context)
    return _PREFLIGHT_CONTEXT


@cd_to_git_root
def save_preflight_context():
    import pickle
    pickle.dump(open('.build_context.pickle', 'wb'), _PREFLIGHT_CONTEXT)


@cd_to_git_root
def list_build_variants():
    build_variants = yaml2json('build-variants.yaml')
    for variant in build_variants['variants']:
        variant = variant['variant']
        yield variant


def find_build_variant(target_variant):
    for variant in list_build_variants():
        if variant['variant_name'] == target_variant:
            return variant


def _get_prop_with_transform(thing, prop_name, **additional_kwargs):
    kwargs = dict(thing)
    kwargs.update(additional_kwargs)
    prop_val = thing.get(prop_name, '')
    if isinstance(prop_val, str):
        return prop_val.format(**kwargs)
    return prop_val


def get_artifact_prop(artifact, prop_name, **additional_kwargs):
    return _get_prop_with_transform(artifact, prop_name, **additional_kwargs)


def get_variant_prop(variant, prop_name, **additional_kwargs):
    return _get_prop_with_transform(variant, prop_name, **additional_kwargs)


def get_named_artifact(artifact_set_name, artifact_name):
    artifacts = get_artifact_set(artifact_set_name)
    for artifact in artifacts:
        if artifact.get('name', '') == artifact_name:
            return artifact
    raise KeyError("Named artifact not found: {}".format(artifact_name))


def get_artifact_set(name):
    def transform(artifact):
        kwargs = dict(artifact)
        br2_dl_dir = os.environ.get('BR2_DL_DIR', 'buildroot/dl')
        kwargs['BR2_DL_DIR'] = br2_dl_dir
        return {K: V.format(**kwargs) for (K, V) in artifact.items()}
    artifacts = yaml2json('external-artifacts.yaml')
    for artifact_set in artifacts['artifact_sets']:
        artifact_set = artifact_set['artifact_set']
        if artifact_set['name'] == name:
            merge_with = []
            if 'merge_with' in artifact_set:
                for mw_set in artifact_set['merge_with']:
                    merge_with.extend(get_artifact_set(mw_set))
            artifacts = artifact_set['artifacts']
            return [transform(V) for V in artifacts + merge_with]
    raise KeyError('could not find artifact set named: {}'.format(name))


def build_s3_artifact_url(artifact):
    s3_bucket = get_artifact_prop(artifact, 's3_bucket')
    s3_repo = get_artifact_prop(artifact, 's3_repository')
    version = get_artifact_prop(artifact, 'version')
    s3_object = get_artifact_prop(artifact, 's3_object')
    return 's3://{s3_bucket}/{s3_repo}/{version}/{s3_object}'.format(
        s3_bucket=s3_bucket,
        s3_repo=s3_repo,
        version=version,
        s3_object=s3_object)


def get_config_hash_path(config_output):
    dirname, basename = os.path.split(config_output)
    hashdest = os.path.join(dirname, "hashes")
    if not os.path.exists(hashdest):
        os.mkdir(hashdest)
    return os.path.join(hashdest, basename)


def yaml2json_filename():
    if platform.machine() == 'x86_64':
        arch = 'amd64'
    else:
        arch = '386'
    if platform.system() == 'Darwin':
        system = 'darwin'
    else:
        system = 'linux'
    bin_name = 'yaml2json_{}_{}'.format(system, arch)
    path = OP.join(OP.dirname(OP.realpath(__file__)),
                   'yaml2json.bin', bin_name)
    return path


def yaml2json(filename):
    """
    Converts yaml to json using the vendored yaml2json tool.
    """
    import json
    yaml2json_tool = yaml2json_filename()
    data = Runner().cat(filename).pipe().call(yaml2json_tool).to_str()
    return json.loads(data)


def sha256digest(filename):
    sha256 = hashlib.new('sha256')
    with open(filename, 'rb') as fp_input:
        sha256.update(fp_input.read())
    digest = sha256.hexdigest()
    return digest


# vim: ft=python:et:sts=4:ts=4:sw=4:tw=120:

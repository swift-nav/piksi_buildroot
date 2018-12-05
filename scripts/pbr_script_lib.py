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
import sys
import subprocess
import urllib
import urlparse

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

    def check(self):
        """
        Run a pipeline and check that it's successful.
        """
        proc = self._make_proc(stderr=subprocess.PIPE)
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
            if os.path.exists('.git'):
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

# vim: ft=python:et:sts=4:ts=4:sw=4:tw=120:

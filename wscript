#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from util import makeconfig
import yaml

APPNAME='zerogw'
VERSION='0.1.3'

top = '.'
out = 'build'

def set_options(opt):
    opt.tool_options('compiler_cc')

def configure(conf):
    conf.check_tool('compiler_cc')

def build(bld):
    bld(
        target='config.h',
        rule=makeheader,
        source='util/parse-config.yaml',
        name='configh',
        )
    bld(
        target='config.c',
        rule=makecode,
        source='util/parse-config.yaml',
        name='configc',
        )
    bld.add_group()
    bld(
        features     = ['cc', 'cprogram'],
        source       = [
            'src/main.c',
            'src/automata.c',
            'config.c',
            'src/log.c',
            ],
        target       = 'zerogw',
        includes     = ['src', bld.bdir + '/default'],
        defines      = [
            'CONFIG_DEBUG',
            'CONFIG_ZEROGW',
            'LOG_STRIP_PATH="../src/"',
            ],
        ccflags      = ['-std=c99', '-g'],
        lib          = ['yaml', 'zmq', 'event'],
        )

    if bld.env['PREFIX'] == '/usr':
        bld.install_files('/etc', ['examples/zerogw.yaml'])
    else:
        bld.install_files('${PREFIX}/etc', ['examples/zerogw.yaml'])

def makeheader(task):
    makeconfig.inityaml()
    src = task.inputs[0].srcpath(task.env)
    tgt = task.outputs[0].bldpath(task.env)
    with open(src, 'rb') as f:
        data = yaml.load(f)
    with open(tgt, 'wt', encoding='utf-8') as f:
        makeconfig.makeheader(data, f, 'zerogw')

def makecode(task):
    makeconfig.inityaml()
    src = task.inputs[0].srcpath(task.env)
    tgt = task.outputs[0].bldpath(task.env)
    with open(src, 'rb') as f:
        data = yaml.load(f)
    with open(tgt, 'wt', encoding='utf-8') as f:
        makeconfig.makecode(data, f, 'zerogw')


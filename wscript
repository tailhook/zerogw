#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import yaml

APPNAME='zerogw'
VERSION='0.3'

top = '.'
out = 'build'

def set_options(opt):
    opt.tool_options('compiler_cc')

def configure(conf):
    conf.check_tool('compiler_cc')

def build(bld):
    import coyaml.waf
    bld(
        features     = ['cc', 'cprogram', 'coyaml'],
        source       = [
            'src/main.c',
            'src/log.c',
            'src/websocket.c',
            'src/sieve.c',
            ],
        target       = 'zerogw',
        includes     = ['src'],
        defines      = [
            'LOG_STRIP_PATH="../src/"',
            ],
        ccflags      = ['-std=c99'],
        lib          = ['yaml', 'zmq', 'ev', 'coyaml', 'website', 'ssl'],
        config       = 'src/config.yaml',
        )

    if bld.env['PREFIX'] == '/usr':
        bld.install_files('/etc', ['examples/zerogw.yaml'])
    else:
        bld.install_files('${PREFIX}/etc', ['examples/zerogw.yaml'])

def makeheader(task):
    import coyaml.cgen, coyaml.hgen, coyaml.core, coyaml.load
    src = task.inputs[0].srcpath(task.env)
    tgt = task.outputs[0].bldpath(task.env)
    cfg = coyaml.core.Config('cfg', os.path.splitext(os.path.basename(tgt))[0])
    with open(src, 'rb') as f:
        coyaml.load.load(f, cfg)
    with open(tgt, 'wt', encoding='utf-8') as f:
        with coyaml.textast.Ast() as ast:
            coyaml.hgen.GenHCode(cfg).make(ast)
        f.write(str(ast))

def makecode(task):
    import coyaml.cgen, coyaml.hgen, coyaml.core, coyaml.load
    src = task.inputs[0].srcpath(task.env)
    tgt = task.outputs[0].bldpath(task.env)
    cfg = coyaml.core.Config('cfg', os.path.splitext(os.path.basename(tgt))[0])
    with open(src, 'rb') as f:
        coyaml.load.load(f, cfg)
    with open(tgt, 'wt', encoding='utf-8') as f:
        with coyaml.textast.Ast() as ast:
            coyaml.cgen.GenCCode(cfg).make(ast)
        f.write(str(ast))

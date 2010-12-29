#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from waflib import Utils, Options
from waflib.Build import BuildContext

APPNAME='zerogw'
VERSION='0.5.1'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')

def build(bld):
    import coyaml.waf
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/config.yaml',
            'src/main.c',
            'src/log.c',
            'src/websocket.c',
            'src/sieve.c',
            'src/zutils.c',
            'src/http.c',
            'src/resolve.c',
            'src/uidgen.c',
            'src/request.c',
            'src/polling.c',
            ],
        target       = 'zerogw',
        includes     = ['src'],
        defines      = [
            'LOG_STRIP_PATH="../src/"',
            ],
        cflags      = ['-std=c99'],
        lib          = ['yaml', 'zmq', 'ev', 'coyaml', 'website', 'ssl'],
        )

    if bld.env['PREFIX'] == '/usr':
        bld.install_files('/etc', ['examples/zerogw.yaml'])
    else:
        bld.install_files('${PREFIX}/etc', ['examples/zerogw.yaml'])

def dist(ctx):
    ctx.excl = ['.waf*', '*.tar.bz2', '*.zip', 'build',
        '.git*', '.lock*', '**/*.pyc']
    ctx.algo = 'tar.bz2'
    
def make_pkgbuild(task):
    import hashlib
    task.outputs[0].write(Utils.subst_vars(task.inputs[0].read(), {
        'VERSION': VERSION,
        'DIST_MD5': hashlib.md5(task.inputs[1].read('rb')).hexdigest(),
        }))
        
def archpkg(ctx):
    from waflib import Options
    Options.commands = ['dist', 'makepkg'] + Options.commands
        
def build_package(bld):
    distfile = APPNAME + '-' + VERSION + '.tar.bz2'
    bld(rule=make_pkgbuild,
        source=['PKGBUILD.tpl', distfile, 'wscript'],
        target='PKGBUILD')
    bld(rule='cp ${SRC} ${TGT}', source=distfile, target='.')
    bld.add_group()
    bld(rule='makepkg -f', source=distfile)
    bld.add_group()
    bld(rule='makepkg -f --source', source=distfile)
    
class makepkg(BuildContext):
    cmd = 'makepkg'
    fun = 'build_package'
    variant = 'archpkg'

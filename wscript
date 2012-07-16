#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from waflib import Utils, Options
from waflib.Build import BuildContext
from waflib.Scripting import Dist
import subprocess
import os.path

APPNAME='zerogw'
if os.path.exists('.git'):
    VERSION=subprocess.getoutput('git describe').lstrip('v').replace('-', '_')
else:
    VERSION='0.6.1'

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
            'src/disk.c',
            'src/commands.c',
            'src/pool.c',
            'src/msgqueue.c',
            ],
        target       = 'zerogw',
        includes     = ['src'],
        defines      = [
            'LOG_STRIP_PATH="../src/"',
            ],
        cflags      = ['-std=gnu99'],
        lib          = [
            'coyaml',
            'yaml',
            'zmq',
            'ev',
            'website',
            'crypto',
            'pthread',
            'm',
            ],
        )
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/config.yaml',
            'src/zerogwctl.c',
            ],
        target       = 'zerogwctl',
        includes     = ['src'],
        cflags       = ['-std=gnu99'],
        lib          = ['coyaml', 'yaml', 'zmq'],
        )
    bld(
        features    = ['c', 'cprogram'],
        source      = 'src/openport.c',
        target      = 'openport',
        )

    if bld.env['PREFIX'] == '/usr':
        bld.install_files('/etc', ['examples/zerogw.yaml'])
        bld.install_as('/etc/bash_completion.d/zerogwctl',
            'completion/bash')
    else:
        bld.install_files('${PREFIX}/etc', ['examples/zerogw.yaml'])
        bld.install_as('${PREFIX}/etc/bash_completion.d/zerogwctl',
            'completion/bash')
    bld.install_as('${PREFIX}/share/zsh/site-functions/_zerogwctl',
        'completion/zsh')

def dist(ctx):
    ctx.excl = [
        'doc/_build/**',
        '.waf*', '*.tar.bz2', '*.zip', 'build',
        '.git*', '.lock*', '**/*.pyc', '**/*.swp', '**/*~'
        'tmp/**',
        ]
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

def encode_multipart_formdata(fields, files):
    """
    fields is a sequence of (name, value) elements for regular form fields.
    files is a sequence of (name, filename, value) elements for data
    to be uploaded as files
    Return (content_type, body) ready for httplib.HTTP instance
    """
    BOUNDARY = b'----------ThIs_Is_tHe_bouNdaRY'
    CRLF = b'\r\n'
    L = []
    for (key, value) in fields:
        L.append(b'--' + BOUNDARY)
        L.append(('Content-Disposition: form-data; name="%s"' % key)
            .encode('utf-8'))
        L.append(b'')
        L.append(value.encode('utf-8'))
    for (key, filename, value, mime) in files:
        assert key == 'file'
        L.append(b'--' + BOUNDARY)
        L.append(b'Content-Type: ' + mime.encode('ascii'))
        L.append(('Content-Disposition: form-data; name="%s"; filename="%s"'
            % (key, filename)).encode('utf-8'))
        L.append(b'')
        L.append(value)
    L.append(b'--' + BOUNDARY + b'--')
    L.append(b'')
    body = CRLF.join(L)
    content_type = 'multipart/form-data; boundary=%s' % BOUNDARY.decode('ascii')
    return content_type, body

def upload(ctx):
    "quick and dirty command to upload files to github"
    import hashlib
    import urllib.parse
    from http.client import HTTPSConnection, HTTPConnection
    import json
    distfile = APPNAME + '-' + VERSION + '.tar.bz2'
    with open(distfile, 'rb') as f:
        distdata = f.read()
    md5 = hashlib.md5(distdata).hexdigest()
    remotes = subprocess.getoutput('git remote -v')
    for r in remotes.splitlines():
        url = r.split()[1]
        if url.startswith('git@github.com:'):
            gh_repo = url[len('git@github.com:'):-len('.git')]
            break
    else:
        raise RuntimeError("repository not found")
    gh_token = subprocess.getoutput('git config github.token').strip()
    gh_login = subprocess.getoutput('git config github.user').strip()
    cli = HTTPSConnection('github.com')
    cli.request('POST', '/'+gh_repo+'/downloads',
        headers={'Host': 'github.com',
                 'Content-Type': 'application/x-www-form-urlencoded'},
        body=urllib.parse.urlencode({
            "file_name": distfile,
            "file_size": len(distdata),
            "description": APPNAME.title() + ' source v'
                + VERSION + " md5: " + md5,
            "login": gh_login,
            "token": gh_token,
        }).encode('utf-8'))
    resp = cli.getresponse()
    data = resp.read().decode('utf-8')
    data = json.loads(data)
    s3data = (
        ("key", data['path']),
        ("acl", data['acl']),
        ("success_action_status", "201"),
        ("Filename", distfile),
        ("AWSAccessKeyId", data['accesskeyid']),
        ("policy", data['policy']),
        ("signature", data['signature']),
        ("Content-Type", data['mime_type']),
        )
    ctype, body = encode_multipart_formdata(s3data, [
        ('file', distfile, distdata, data['mime_type']),
        ])
    cli.close()
    cli = HTTPSConnection('github.s3.amazonaws.com')
    cli.request('POST', '/',
                body=body,
                headers={'Content-Type': ctype,
                         'Host': 'github.s3.amazonaws.com'})
    resp = cli.getresponse()
    print(resp.read())

def build_tests(bld):
    build(bld)
    bld.add_group()
    bld(rule='cd ${SRC[0].parent.parent.abspath()};'
        'export BUILDDIR=${SRC[1].parent.abspath()};'
        'python -m unittest discover -s test -p "*.py" -t . -v',
        source=['test/simple.py', 'zerogw'],
        always=True)

class test(BuildContext):
    cmd = 'test'
    fun = 'build_tests'

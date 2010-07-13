#!/usr/bin/env python3
import sys
import optparse
import re
import inspect
import struct
import socket
import string
import copy
from collections import deque
from io import BytesIO, StringIO

import yaml
import jinja2

from .conf import CmdLine, TcpSocket, UnixSocket
from . import conf

jinenv = jinja2.Environment(extensions=['jinja2.ext.do'])

header_tpl = jinenv.from_string(r"""
/* WARNING: THIS IS AUTOMATICALLY GENERATED FILE, DO NOT EDIT! */

{%- macro tail(value, name) %}
{%- if not (value, name) in tailset %}
{{- tailset.add((value, name)) or '' }}
{{- tailtypes.append((value, name)) or '' }}
{%- endif %}
{%- endmacro%}

{%- macro complex_type(valtype, name, level=1) %}
{%- if isinstance(valtype, dict) %}
{{'    '*level}}struct {
{%- for (k, v) in valtype.items() %}
{{'    '*level}}    {{ v.c_type(k) if isinstance(v, Property) else complex_type(v, varname(k), level+1) }};
{%- endfor %}
{{'    '*level}}} {{ name -}}
{%- elif isinstance(valtype, Array) %}
{{'    '*level}}struct config_{{ smallname }}_{{ name }}_s *{{ name -}}
{{- tail(valtype, name) or '' -}}
{%- elif isinstance(valtype, Mapping) %}
{{'    '*level}}struct config_{{ smallname }}_{{ name }}_s *{{ name -}}
{{- tail(valtype, name) -}}
{% else %}
{{- print(valtype) -}}
{{- valtype/0 -}}
{%- endif %}
{%- endmacro %}

{%- set bigname = cname|upper %}
{%- set smallname = cname|lower %}
{%- set tailtypes = [] %}
{%- set tailset = set() %}
#ifndef _H_CFG_{{ bigname }}
#define _H_CFG_{{ bigname }}

#include <stddef.h>
#include "configbase.h"

typedef struct config_{{ smallname }}_s {
    config_head_t head;
{%- for (k, v) in data.items() %}
    {{- v.c_type(k) if isinstance(v, Property) else complex_type(v, varname(k)) }};
{%- endfor %}
} config_{{ smallname }}_t;

{% for (valtype, name) in tailtypes %}
typedef struct config_{{ smallname }}_{{ name }}_s {
{%- if isinstance(valtype, Mapping) %}
    mapping_element_t head;
{%- else %}
    array_element_t head;
{%- endif %}
{%- if isinstance(valtype.element, dict) %}
    {% for (k, v) in valtype.element.items() %}
        {{- v.c_type(k) if isinstance(v, Property) else complex_type(v, varname(k)) }};
    {% endfor %}
{%- else %}
    {{ valtype.element.c_type("value") }};
{%- endif %}
} config_{{ smallname }}_{{ name }}_t;
{% endfor %}

#if defined(CONFIG_{{ bigname }})
    typedef config_{{ smallname }}_t config_t;
    extern config_t config;
#endif
#endif
""")

source_tpl = jinja2.Template(r"""
/* WARNING: THIS IS AUTOMATICALLY GENERATED FILE, DO NOT EDIT! */
{% set bigname = cname|upper -%}
{%- set smallname = cname|lower -%}

#define _GNU_SOURCE
#define _BSD_SOURCE
#define IN_CONFIG_C
#include <unistd.h>

#include "config.h"
#include "log.h"

#include "configbase.c"

static char *optstr_{{ smallname }} = "{{ cmdline.optstring() }}";
static struct option options_{{ smallname }}[] = {
{%- for (i, o) in enumerate(cmdline.options()) %}
{%- for oname in o._long_opts %}
    { "{{ oname.lstrip('-') }}", {{ str(bool(getattr(o, 'dest', None))).upper()
        }}, NULL, {{ 1000+i }} },
{%- endfor %}
{%- endfor %}
    { NULL, 0, NULL, 0 } };

/*
    Each state has a pointer (offset) to where to store value. Each state has a
    type of the value. Including imaginary types like struct. Type is just
    a function which is called on event in this state. It usually just checks
    type and goes into inner or outer state (for mappings, sequences). Arrays
    and Mappings has slightly different meaning for offset, it's offset inside
    the element. Also stack of arrays/mappings and obstack for memory allocation
    are passed to state functions.
*/

typedef enum {
{%- for (i, s) in enumerate(options.states()) %}
    {{ s.id }}, //#{{ i }}
{%- endfor %}
    CS_{{ bigname }}_STATES_TOTAL,
} config_{{ smallname }}_state_t;

{%- for (key, array) in options.arrays() %}
{%- if array.default_properties %}
static void config_defaults_{{ smallname }}_{{ constname(key) }}(void *cfg, void *el);
{%- endif %}
{%- endfor %}

{% set idx = [0] %}
static struct_transition_t transitions_{{ smallname }}[] = {
{%- for m in options.structs() %}
    {{- setattr(m, 'transition_index', idx[0]) or '' -}}
    {%- for (key, state) in m.transitions() %}
    { "{{ key }}", {{ state.id }} }, {{- idx.__setitem__(0, idx[0]+1) or '' -}}
    {%- endfor %}
    { NULL, 0 }, {{- idx.__setitem__(0, idx[0]+1) or '' -}}
{%- endfor %}
    };

static state_info_t states_{{ smallname }}[] = {
{%- for s in options.states() %}
    { {{ s.c_info() }} },
{%- endfor %}
    { id: -1 } };

static void config_defaults_{{ smallname }}(void *cfg) {
    config_{{ smallname }}_t *config = cfg;
{%- for s in options.states() %}
{%- if hasattr(s.property, 'c_set_default')
    and hasattr(s.property, 'default')
    and s.path[0] == smallname %}
    {{ s.property.c_set_default(s) }};
{%- endif %}
{%- endfor %}
}

{%- for (key, array) in options.arrays() %}
{%- if array.default_properties %}
static void config_defaults_{{ smallname }}_{{ constname(key) }}(void *cfg, void *el) {
    config_{{ smallname }}_t *config = cfg;
    config_{{ smallname }}_{{ key }}_t *element = el;
    {%- for (s1, s2) in options.array_pairs(array) %}
    {{ s1.c_set(s2) }};
    {%- endfor %}
}
{%- endif %}
{%- endfor %}


void read_options_{{ smallname }}(int argc, char **argv, void *cfg) {
    config_{{ smallname}}_t *config = cfg;
    int o;
    optind = 1;
    while((o = getopt_long(argc, argv, optstr_{{ smallname }},
        options_{{ smallname }}, NULL)) != -1) {
        switch(o) {
{% for (i, o) in enumerate(cmdline.options()) %}
        case {{ 1000+i }}:
    {%- for opt in o._short_opts %}
        case '{{ opt[1] }}':
    {%- endfor %}
    {%- if o.dest == 'config' %}
            config_filename = optarg;
    {%- elif o.dest %}
        {%- if o.type == 'float' %}
            if(config) {
                config->{{ o.dest.replace('-', '_') }} = atof(optarg); //TODO: check
            }
        {%- elif o.type == 'int' %}
            if(config) {
                config->{{ o.dest.replace('-', '_') }} = atoi(optarg); //TODO: check
            }
        {%- elif o.type == 'uint' %}
            if(config) {
                config->{{ o.dest.replace('-', '_') }} = atoi(optarg); //TODO: check
                ANIMPL(config->{{ o.dest.replace('-', '_') }} >= 0);
            }
        {%- elif o.type == 'string' %}
            if(config) {
                config->{{ o.dest.replace('-', '_') }} = optarg;
                config->{{ o.dest.replace('-', '_') }}_len = strlen(optarg);
            }
        {%- else %}
        {{ o.type/1 }}
        {%- endif %}
    {%- elif o.action == "help" %}
            printf(
        {%- for line in cmdline.format_help().splitlines() %}
                "{{ line.replace('"', '\\"') }}\n"
        {%- endfor %}
                );
            exit(0);
    {%- endif %}
            break;
{%- endfor %}
        default:
            fprintf(stderr, "Usage: %s [options]\n", argv[0]);
            exit(1);
        }
    }
}

#if defined(CONFIG_{{ bigname }})
    config_defaults_func_t config_defaults = config_defaults_{{ smallname }};
    read_options_func_t read_options = read_options_{{ smallname }};
    char *config_name = "{{ name.capitalize() }}";
    state_info_t *config_states = states_{{ smallname }};
    config_meta_t config_meta = {
        config_defaults: config_defaults_{{ smallname }},
        read_options: read_options_{{ smallname }},
        service_name: "{{ cname.capitalize() }}",
        config_states: states_{{ smallname }},
        };
#endif
""")

keywords = frozenset('default int class'.split())

def varname(a):
    a = a.replace('-', '_')
    if a in keywords:
        return a + '_'
    else:
        return a

def propname(a):
    if isinstance(a, (list, tuple)):
        return '.'.join(map(varname, a))
    return varname(a)

def constname(*a):
    if len(a) == 1: a = a[0]
    if isinstance(a, (list, tuple)):
        return '_'.join(map(constname, list(filter(bool, a))))
    return varname(a)

def cstring(v):
    f = StringIO()
    if '\n' in v:
        f.write('\n    ')
    f.write('"')
    for c in v:
        if c == '\\':
            f.write(r'\\')
        elif c == '"':
            f.write(r'\"')
        elif c == "\r":
            f.write(r'\r')
        elif c == "\n":
            f.write('\\n"\n    "')
        elif c in string.printable:
            f.write(str(c))
        else:
            f.write(r'\x%02x' % ord(c))
    f.write('"')
    return f.getvalue()

class Array(conf.Array):
    def c_info(self):
        return {
            'inner_state': self.inner_state.id,
            'element_size': 'sizeof(config_%s_t)'
                % constname(self.inner_state.path[0]),
            'current_element': 'NULL',
            'defaults_fun': 'config_defaults_%s_%s'
                % tuple(map(constname, (self.state.path[0], self.state.path[-1])))
                if self.default_properties else {},
            }

class Mapping(conf.Mapping):
    def c_info(self):
        return {
            'inner_state': self.inner_state.id,
            'element_size': 'sizeof(config_%s_t)'
                % constname(self.inner_state.path[0]),
            'current_element': 'NULL',
            'defaults_fun': 'config_defaults_%s_%s'
                % tuple(map(constname, (self.state.path[0], self.state.path[-1])))
                if self.default_properties else {},
            }

class Property(conf.Property):
    _c_typemap = dict(
        uint='unsigned int',
        string=(('char *', ''), ('size_t', '_len')),
        file=(('char *', ''), ('size_t', '_len')),
        dir=(('char *', ''), ('size_t', '_len')),
        listen=(('struct sockaddr *', ''), ('size_t', '_len')),
        service=(('struct sockaddr *', ''), ('size_t', '_len')),
        group='gid_t',
        user='uid_t',
        )

    def func(self):
        return self.type

    def c_type(self, name):
        name = varname(name)
        if self.type == 'hex':
            return 'char %s[%d]' % (name, self.length)
        v = self._c_typemap.get(self.type)
        if v is None:
            return '%s %s' % (self.type, name)
        elif isinstance(v, tuple):
            return '; '.join('%s %s%s' % (t, name, s) for (t, s) in v)
        else:
            return '%s %s' % (v, name)

    def c_info(self):
        e = set(('description', 'type', 'state',
            'command_line', 'command_line_incr', 'command_line_decr',
            ))
        r = {}
        for (k, v) in self.__dict__.items():
            if k.startswith('_') or k in e: continue
            self._c_key(self.state.path[0], r, k, v, is_value=k == 'default')
        if self.type in ('int', 'uint', 'float', 'bool'):
            r['bitmask'] = str(0
                | (1 if 'default' in r else 0)
                | (2 if 'min' in r else 0)
                | (4 if 'max' in r else 0)
                )
        return r

    def _c_key(self, component, r, k, v, is_value=False):
        k = varname(k)
        if self.type in ('listen', 'service') and k == 'model':
            r[k] = 'LISTENING_MODEL_' + v.upper()
        elif self.type in ('string', 'dir', 'file') and is_value:
            r[k] = cstring(v)
            r[k + '_len'] = len(v)
        elif isinstance(v, str):
            r[k] = cstring(v)
        elif isinstance(v, bool):
            r[k] = str(v).upper()
        elif isinstance(v, TcpSocket):
            r[k] = 'NULL' # TODO
        else:
            r[k] = repr(v)

    def c_set_default(self, st):
        var = propname(st.path[1:])
        if self.type == 'user':
            return '{ struct passwd user; struct passwd *tmp;' \
                ' if(getpwnam_r(%s, &user, NULL, 0, &tmp)) ' \
                ' config->%s = user.pw_uid; }' % (cstring(self.default), var)
        elif self.type == 'group':
            return '{ struct group group; struct group *tmp;' \
                ' if(getgrnam_r(%s, &group, NULL, 0, &tmp)) ' \
                ' config->%s = group.gr_gid; }' % (cstring(self.default), var)
            return
        r = {}
        self._c_key(self.state.path[0], r, var, self.default, True)
        return '; '.join('config->%s = %s' % (k, v) for (k, v) in r.items())
    def c_set(self, dest):
        src = propname(self.state.path[1:])
        dest = propname(dest.state.path[1:])
        if self.type in ('listen', 'service', 'string', 'file', 'dir'):
            return 'element->%s = config->%s; element->%s_len = config->%s_len' \
                % (src, dest, src, dest)
        else:
            return 'element->%s = config->%s' \
                % (src, dest)

order_them_ = {
    'id': '000',
    'return_state': '010',
    }
def order_them(pair):
    return order_them_.get(pair[0], pair[0])

def c_serialize(d):
    r = []
    for (k, v) in sorted(iter(d.items()), key=order_them):
        if v == {}: continue
        if isinstance(v, dict):
            v = c_serialize(v)
            if not v: continue
            v = '{ %s }' % v
        r.append('%s: %s' % (k, v))
    return ', '.join(r)


class State(object):
    _all_states = {}

    def __init__(self, path, return_state, type, property):
        self.path = path
        self.type = type
        self.return_state = return_state
        self.property = property
        self.children = []
        if return_state:
            return_state.children.append(self)
        if not isinstance(property, dict):
            property.state = self

    def get_id(self):
        val = constname('CS', self.path[0].upper(), self.path[1:])
        if self._all_states.setdefault(val, self) is self:
            return val
        i = 2
        while self._all_states.setdefault(val+'_'+str(i), self) is not self:
            i += 1
        return val + '_' + str(i)
    id = property(get_id)

    def c_info(self):
        return c_serialize({
            'id': self.id,
            'entry_type': constname('CONFIG', self.type.upper()),
            'func': 'config_state_' + self.type,
            'return_state': self.return_state.id if self.return_state else '-1',
            'offset': self.offset(),
            'options': { 'o_' + self.type: self.property.c_info()
                if hasattr(self.property, 'c_info') else {} },
            })

    def offset(self):
        return 'offsetof(config_%s_t, %s)' \
            % (constname(self.path[0]), propname(self.path[1:]) or 'head')

class StateEl(State):
    def offset(self):
        return 'offsetof(config_%s_t, %s)' \
            % (constname(self.path[0]), 'value'
                if self.type != 'struct' else 'head')

class StateAr(State):
    pass

class InternalMapping(object):

    def __init__(self, mapping, state):
        self.mapping = mapping
        self.state = state
        state.property = self
        self.state_map = {}
        for (k, v) in self.mapping.items():
            p = state.path + (k,)
            if hasattr(v, 'state'):
                v = copy.deepcopy(v)
            if isinstance(v, (Array, Mapping)):
                ms = StateAr(p, state, v.type,
                    property=v)
                self.state_map[k] = ms
                el = v.element
                p = (state.path[0].split('_')[0] + '_' + k,)
                if isinstance(el, dict):
                    ins = StateEl(p, ms, 'struct', property=el)
                else:
                    ins = StateEl(p, ms, el.type, property=el)
                self.state_map[k + '_el'] = ins
                v.inner_state = ins
            elif isinstance(v, dict):
                self.state_map[k] = State(p, state, 'struct', property=v)
            else:
                self.state_map[k] = State(p, state, v.type, property=v)

    def transitions(self):
        for (k, v) in self.state_map.items():
            if isinstance(v, StateEl): continue
            yield k, v

    def states(self):
        return iter(self.state_map.items())

    @property
    def statename(self):
        return self.state.name

    def c_info(self):
        return { 'transitions': '&transitions_%s[%d]'
            % (self.state.path[0].split('_')[0], self.transition_index) }

class Options(object):
    def __init__(self, component, data):
        self._mappings = []
        self._arrays = {}
        self._states = [State((component,), None, 'struct', data)]
        self._root = InternalMapping(data, self._states[0])
        buf = deque([self._root])
        while buf:
            map = buf.popleft()
            self._mappings.append(map)
            for (k, s) in map.states():
                if s.type == 'struct':
                    buf.append(InternalMapping(s.property, s))
                self._states.append(s)
                if s.type in ('array', 'mapping'):
                    self._arrays[varname(k)] =  s.property
        self.set_socket_string()

    def set_socket_string(self):
        f = BytesIO()
        for i in self._states:
            if i.type in ('service', 'listen') and \
                hasattr(i.property, 'default'):
                i.property._socket_index = f.tell()
                if isinstance(i.property.default, UnixSocket):
                    f.write(struct.pack('<H', socket.AF_UNIX))
                    f.write(i.property.default.value.encode('utf-8'))
                    f.write('\0')
                else:
                    host, port = i.property.default.value.split(':')
                    f.write(struct.pack('<H', socket.AF_INET))
                    f.write(struct.pack('>H', int(port)))
                    f.write(struct.pack('BBBB',
                        *reversed(__builtins__.map(int, host.split('.')))))
                i.property._socket_length = f.tell() - i.property._socket_index
        self.socket_string = f.getvalue()

    def structs(self):
        return iter(self._mappings)

    def states(self):
        return iter(self._states)

    def arrays(self):
        return iter(self._arrays.items())

    def array_pairs(self, ar):
        for p in ar.state.return_state.children:
            if p.path[-1] == ar.default_properties:
                break
        else:
            raise KeyError(ar.default_properties)
        for pair in self._pairs(ar.inner_state, p):
            yield pair

    def _pairs(self, a, b):
        for k in a.children:
            for v in b.children:
                if k.path[-1] == v.path[-1]:
                    assert k.type == v.type
                    if k.type == 'struct':
                        for pair in self._pairs(k, v):
                            yield pair
                    elif k.type not in ('mapping', 'array'):
                        yield k.property, v.property
                    break

def writesock(Dumper, value):
    return Dumper.represent_scalar(
        '!Tcp' if isinstance(value, TcpSocket) else '!Unix',
        value.value)

def uni_convert(Dumper, value):
    return Dumper.represent_data(value.encode('utf-8'))

def inityaml():
    import os.path
    global source_dir

    conf._inityaml()

    source_dir = os.path.dirname(os.path.dirname(__file__))
    yaml.add_constructor("!Array", Array)
    yaml.add_constructor("!Mapping", Mapping)
    yaml.add_constructor("!Property", Property)

    yaml.add_representer(TcpSocket, writesock)
    yaml.add_representer(UnixSocket, writesock)
    #~ yaml.add_representer(str, uni_convert)

#~ def makemain(data, file):
    #~ file.write(main_tpl.render_unicode(data=data))

def makecode(data, file, name):
    file.write(source_tpl.render(
        options=Options(name.lower(), data),
        cmdline=CmdLine(data),
        name=name,
        cname=name,
        **stdvars()))

def stdvars():
    return dict(
        isinstance=isinstance,
        Property=Property, Array=Array, Mapping=Mapping, varname=varname,
        dict=dict, set=set, enumerate=enumerate, getattr=getattr, bool=bool,
        str=str, setattr=setattr, cstring=cstring, hasattr=hasattr,
        print=print, #for debugging
        )

def makeheader(data, file, name):
    file.write(header_tpl.render(data=data, cname=name, **stdvars()))

def word_wrap(string, width=80, indent=0, prefix=''):
    """ word wrapping function.
        string: the string to wrap
        width: the column number to wrap at
        prefix: prefix each line with this string (goes before any indentation)
        indent: number of characters to indent before prefix
    """
    string = indent*" " + prefix + string
    newstring = ""
    if len(string) > width:
        while True:
            # find position of nearest whitespace char to the left of "width"
            marker = width-indent-len(prefix)
            while not string[marker].isspace():
                marker = marker - 1

            # remove line from original string and add it to the new string
            newline = string[0:marker] + "\n"
            newstring = newstring + newline
            string = " "*indent + prefix + string[marker+1:]

            # break out of loop when finished
            if len(string) <= width:
                break

    return newstring + string

def alterdata(data, filter=None, replace=None, alter=None):
    for (k, v) in list(data.items()):
        if isinstance(v, dict):
            alterdata(v, filter=filter, replace=replace)
            if not v:
                del data[k]
        elif isinstance(v, (Property, Array, Mapping)):
            if filter is not None:
                if not filter(v):
                    del data[k]
                    continue
            if alter is not None:
                alter(v)
            if replace is not None:
                data[k] = replace(v)

def makeexample(data, file, mode):
    import re
    reob = re.compile('<ob:([0-9a-fA-F]+)>:')
    if mode == 'defaults':
        alterdata(data, filter=lambda v: hasattr(v, 'default'),
            replace=lambda v: v.default)
        yaml.dump(data, file, default_flow_style=False, allow_unicode=True)
    elif mode == 'minimal':
        alterdata(data, filter=lambda v: not hasattr(v, 'default'),
            replace=lambda v: '(%s)' % v.type)
        yaml.dump(data, file, default_flow_style=False, allow_unicode=True)
    elif mode == 'full':
        objects = {}
        def powerfull_replace(v):
            value = v.default if hasattr(v, 'default') else '(%s)' % v.type
            if getattr(v, 'description', None):
                objects[id(v)] = v
                if isinstance(value, TcpSocket):
                    return '<ob:%x>:!Tcp "%s"' % (id(v), value.value)
                elif isinstance(value, UnixSocket):
                    return '<ob:%x>:!Unix "%s"' % (id(v), value.value)
                else:
                    return '<ob:%x>:%s' % (id(v), value)
            else:
                return value
        def powerfull_subs(m):
            prop = objects[int(m.group(1), 16)]
            file.write(word_wrap(prop.description, indent=indent, prefix='# '))
            return ""
        alterdata(data, replace=powerfull_replace)
        s = StringIO()
        yaml.dump(data, s, default_flow_style=False, allow_unicode=True)
        s.seek(0, 0)
        for line in s:
            if not line.startswith(' '):
                file.write('\n')
            if '<ob:' in line:
                indent = 0
                for c in line:
                    if c != ' ': break
                    indent += 1
                line = reob.sub(powerfull_subs, line)
            file.write(line)

def main():
    import sys
    from optparse import OptionParser
    op = OptionParser(usage="%prog [options] config.yaml")
    op.add_option("-e", "--header",
        help="Generate header for configuration",
        dest="action", action="store_const", const="header")
    op.add_option("-c", "--code",
        help="Generate code for configuration",
        dest="action", action="store_const", const="code")
    op.add_option("-x", "--example", metavar="MODE",
        help="Generate example config file. MODE is `full` for commented out"
            " all options. `minimal` for minimal configuration file, `defaults`"
            " for configuration file with all options set to defaults",
        dest="example", default=None, type="choice",
        choices=('full', 'minimal', 'defaults'))
    op.add_option("-o", "--output", metavar="FILE",
        help="Write output into a file, instead of stdout",
        dest="output", default=None, type="string")
    op.add_option("-n", "--name", metavar="NAME",
        help="Name of configuration structures",
        dest="name", default="zerogw", type="string")
    options, args = op.parse_args()
    if len(args) > 1:
        print("Too many arguments", file=sys.stderr)
        sys.exit(1)

    inityaml()
    if len(args):
        f = open(args[0], 'rt', encoding='utf-8')
    else:
        f = sys.stdin
    with f:
        data = yaml.load(f)

    if options.output:
        o = open(options.output, 'wt', encoding='utf-8')
    else:
        o = sys.stdout
    with o:
        if options.action == 'header':
            makeheader(data, o, options.name)
        elif options.action == 'code':
            makecode(data, o, options.name)
        if options.example:
            makeexample(data, o, options.example)

if __name__ == '__main__':
    main()

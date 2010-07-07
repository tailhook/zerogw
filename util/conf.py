import sys, os
import optparse
import copy
import yaml
import inspect
from time import strftime
from functools import partial

class TcpSocket(object):

    def __init__(self, Loader, node):
        self.value = Loader.construct_scalar(node)

    def for_py(self):
        a = self.value.split(':', 1)
        a[1] = int(a[1])
        return tuple(a)

class UnixSocket(object):

    def __init__(self, Loader, node):
        self.value = Loader.construct_scalar(node)

    def for_py(self):
        return self.value

class CmdLine(object):
    class Option(optparse.Option):
        def check_listen(option, opt, value):
            return value
        TYPES = optparse.Option.TYPES + ("listen",)
        TYPE_CHECKER = copy.copy(optparse.Option.TYPE_CHECKER)
        TYPE_CHECKER["listen"] = check_listen

    def __init__(self, data):
        self.op = optparse.OptionParser(usage="zerogw [options]",
            description="Zerogw is an HTTP/zeromq gateway",
            option_class=self.Option)
        self.op.add_option('-c', '--config', metavar="FILE",
            help="Configuration file (default \"/etc/zerogw.yaml\")",
            dest="config", default="/etc/zerogw.yaml")
        self.collect(data)

    def collect(self, data, prefix=''):
        for (k, v) in list(data.items()):
            n = prefix + k.replace('-', '_')
            if isinstance(v, Property):
                l = getattr(v, 'command_line', None)
                if l:
                    if isinstance(l, str):
                        l = (l,)
                    self.op.add_option(*l, dest=n, type=v.py_type(),
                        metavar=n.rsplit('.', 1)[1].upper(),
                        help=v.description, default=getattr(v, 'default', None))
            elif isinstance(v, dict):
                self.collect(v, n + '.')

    def optstring(self):
        return ''.join(s[1] + ':' if getattr(o, 'dest', None) else s[1]
            for o in self.op.option_list for s in o._short_opts)

    def options(self):
        return self.op.option_list

    def format_help(self):
        # sorry, can't use self.op.format_help, because f*cking "waf"
        # patches it :(
        fmt = self.op.formatter
        result = []
        if self.op.usage:
            result.append(fmt.format_usage(self.op.usage) + "\n")
        if self.op.description:
            result.append(self.op.format_description(fmt) + "\n")
        result.append(self.op.format_option_help(fmt))
        result.append(self.op.format_epilog(fmt))
        return "".join(result)

class PropBase(object):
    def make_subs(self, val, Loader):
            if isinstance(val, str):
                return val \
                    .replace('{source-dir}', source_dir) \
                    .replace('{component}', Loader.component)
            elif isinstance(val, UnixSocket):
                val.value = val.value \
                    .replace('{source-dir}', source_dir) \
                    .replace('{component}', Loader.component)
            elif isinstance(val, dict):
                for (k, v) in list(val.items()):
                    val[k] = self.make_subs(v, Loader)
            return val

class Property(PropBase):
    _py_typemap = dict(
        uint='int',
        string='str',
        file='str',
        dir='str',
        listen='listen',
        service='str',
        group='str',
        user='str',
        )
    def __init__(self, Loader, node):
        for (k, v) in list(Loader.construct_mapping(node).items()):
            setattr(self, k.replace('-', '_'), v)
        if not hasattr(self, 'type'):
            raise yaml.error.MarkedYAMLError(context_mark=node.start_mark,
                problem="Property *must* have type attribute")
        if hasattr(self, 'default'):
            self.default = self.make_subs(self.default, Loader)
    def py_type(self):
        return self._py_typemap.get(self.type, self.type)

class Array(PropBase):
    def __init__(self, Loader, node):
        self.default_properties = None
        for (k, v) in list(Loader.construct_mapping(node).items()):
            setattr(self, k.replace('-', '_'), v)
        self.type = 'array'
        if hasattr(self, 'default'):
            self.default = self.make_subs(self.default, Loader)

class Mapping(PropBase):
    def __init__(self, Loader, node):
        self.default_properties = None
        for (k, v) in list(Loader.construct_mapping(node).items()):
            setattr(self, k.replace('-', '_'), v)
        self.type = 'mapping'
        if hasattr(self, 'default'):
            self.default = self.make_subs(self.default, Loader)

def component(Loader, node):
    Loader.constructed_objects = {}
    return Loader.construct_mapping(node, deep=True)

def component_name(Loader, node):
    res = Loader.construct_scalar(node)
    Loader.component = res.lower()
    return res

def _inityaml():
    import os.path
    global source_dir
    source_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    yaml.add_constructor("!Array", Array)
    yaml.add_constructor("!Mapping", Mapping)
    yaml.add_constructor("!Property", Property)
    # types for default values
    yaml.add_constructor("!Tcp", TcpSocket)
    yaml.add_constructor("!Unix", UnixSocket)
    # special type
    yaml.add_constructor("!Component", component)
    yaml.add_constructor("!ComponentName", component_name)
    yaml.Loader.add_path_resolver('!Component', [None], dict)
    yaml.Loader.add_path_resolver('!ComponentName', [True], str)

def _openlogs():
    global LOGFILE
    LOGFILE = open(logging.file, "a", 1)
    os.dup2(LOGFILE.fileno(), 1)
    os.dup2(LOGFILE.fileno(), 2)

class Struct(dict):
    __slots__ = ()
    def __getattr__(self, name):
        return self[name]
    def update(self, values):
        for (k, v) in list(values.items()):
            if isinstance(v, dict):
                self[k].update(v)
            else:
                self[k] = v
    @classmethod
    def create(C, src):
        self = C()
        for (k, v) in list(src.items()):
            if isinstance(v, dict):
                self[_key(k)] = Struct.create(v)
            elif hasattr(v, 'default'):
                self[_key(k)] = v.default
        return self
def _key(k):
    return k.replace('-', '_')

def _read_config(filename, template):
    import proto.conf
    for (k, v) in list(template[service].items()):
        if isinstance(v, dict):
            setattr(proto.conf, _key(k), Struct.create(v))
        elif hasattr(v, 'default'):
            setattr(proto.conf, _key(k), v.default)
    with open(filename) as f:
        data = yaml.load(f)
    for (k, v) in list(data.get('Global', {}).items()):
        if isinstance(v, dict):
            if hasattr(proto.conf, _key(k)):
                getattr(proto.conf, _key(k)).update(v)
        else:
            setattr(proto.conf, _key(k), v)
    for (k, v) in list(data.get(service, {}).items()):
        if isinstance(v, dict):
            getattr(proto.conf, _key(k)).update(v)
        else:
            setattr(proto.conf, _key(k), v)

def mysqlconnect():
    import MySQLdb
    return MySQLdb.connect(services.mysql.host, services.mysql.user,
        services.mysql.password, services.mysql.database, charset='utf8',
        unix_socket=services.mysql.socket)

def init(svc, open_logs=True):
    global service, config
    service = svc
    _inityaml()
    with open(os.path.join(source_dir, 'src', 'parse-config.yaml')) as f:
        data = yaml.load(f)
    cmd = CmdLine(data[svc])
    options, args = cmd.op.parse_args()
    _read_config(options.config, data)
    if open_logs:
        _openlogs()
    from proto import conf
    for (name, id, code) in LOG_LEVELS:
        setattr(conf, 'L' + name, partial(LOG, id))
        setattr(conf, 'A' + name, partial(ASSERT, id))
    return args

def LOG(level, msg, *args):
    if level > logging.level: return
    lvl = LOG_LEVELS[level][2]
    frm = inspect.currentframe(1)
    fname = os.path.basename(inspect.getfile(frm))
    msg = str(msg) % args
    LOGFILE.write('%s [%s] %s:%s: %s\n'
        % (strftime('%Y-%m-%d %H:%M:%S'), lvl, fname, frm.f_lineno, msg))
    if level <= logging.errlevel: sys.exit(1)

def ASSERT(level, cond, msg, *args):
    if cond: return
    if level > logging.level: return
    lvl = LOG_LEVELS[level][2]
    frm = inspect.currentframe(1)
    fname = os.path.basename(inspect.getfile(frm))
    LOGFILE.write('%s [%s] %s:%s: %s\n'
        % (strftime('%Y-%m-%d %H:%M:%S'), lvl, fname, frm.f_lineno, msg))
    if level <= logging.errlevel: sys.exit(1)

LOG_LEVELS = (
    ('EMERG',   0,  'EMRG'),
    ('ALERT',   1,  'ALRT'),
    ('CRIT',    2,  'CRIT'),
    ('ERR',     3,  'ERRR'),
    ('WARN',    4,  'WARN'),
    ('NOTICE',  5,  'NOTE'),
    ('INFO',    6,  'INFO'),
    ('DEBUG',   7,  'DEBG'),
    )

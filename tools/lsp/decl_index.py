"""Declaration symbols come exclusively from the core's --decl-json parser."""
import pathlib
import subprocess
import tempfile
import json
import time

MAX_BYTES = 2_000_000
MAX_FILES = 64
TIMEOUT = 5


def is_declaration(uri):
    return uri.lower().endswith('.d.tjs')


def byte_position(text, offset):
    before = text.encode('utf-8')[:offset].decode('utf-8')
    return {'line': before.count('\n'),
            'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}


def signature(symbol):
    kind, name = symbol['kind'], symbol['name']
    if kind in ('function', 'static'):
        params = []
        for index, param in enumerate(symbol['params']):
            value = param['name'] or ('' if param['collapse'] else f'arg{index}')
            if param['type']:
                value += ': ' + param['type']
            if param['collapse']:
                value += '*'
            elif param['optional']:
                value += ' = ' + param['defaultText']
            params.append(value)
        return ('static ' if kind == 'static' else '') + name + '(' + ', '.join(params) + ')' + (
            ': ' + symbol['ret'] if symbol['ret'] else '')
    if kind == 'class':
        return 'class ' + name + (' extends ' + ', '.join(symbol['extends']) if symbol['extends'] else '')
    if kind == 'namespace':
        return 'namespace ' + name
    access = []
    if kind == 'property':
        if symbol['hasGetter']:
            access.append('get')
        if symbol['hasSetter']:
            access.append('set')
    return name + (': ' + symbol['ret'] if symbol['ret'] else '') + (
        ' (' + ', '.join(access) + ')' if access else '')


class DeclarationIndex:
    def __init__(self, cial):
        self.cial = cial
        self.cache = {}
        self.standard = []
        self.entries = {}
        self.directory = None
        self.attempted_standard = False

    def close(self):
        if self.directory is not None:
            self.directory.cleanup()
            self.directory = None

    def parse(self, uri, text, timeout=TIMEOUT):
        if len(text.encode('utf-8')) > MAX_BYTES:
            return []
        cached = self.cache.get(uri)
        if cached is not None and cached[0] == text:
            return cached[1]
        try:
            run = subprocess.run([self.cial, '--decl-json', '--source-name', uri, '-'],
                                 input=text.encode('utf-8'), capture_output=True, timeout=timeout)
            report = json.loads(run.stdout)
            if run.returncode not in (0, 1) or report.get('schemaVersion') != 1:
                return None
            if not report['check']['ok']:
                symbols = []  # Invalid declarations never leak partial API/type facts.
            else:
                symbols = report['symbols']
        except (OSError, ValueError, KeyError, subprocess.TimeoutExpired):
            return None  # A transient tool failure must be retried, not cached as an empty API.
        entries = []

        def visit(nodes, owner=''):
            for node in nodes:
                qualified = owner + '.' + node['name'] if owner else node['name']
                span = {'start': byte_position(text, node['begin']), 'end': byte_position(text, node['end'])}
                entry = {'symbol': node, 'qualified': qualified, 'owner': owner, 'uri': uri, 'range': span}
                entries.append(entry)
                visit(node['members'], qualified)

        try:
            visit(symbols)
        except (KeyError, TypeError, UnicodeError):
            entries = []
        self.cache[uri] = (text, entries)
        return entries

    def load_standard(self):
        if self.attempted_standard:
            return
        self.attempted_standard = True
        try:
            run = subprocess.run([self.cial, '--api-decl'], capture_output=True, timeout=TIMEOUT)
            if run.returncode != 0 or len(run.stdout) > MAX_BYTES:
                return
            text = run.stdout.decode('utf-8')
            self.directory = tempfile.TemporaryDirectory(prefix='cial-lsp-declarations-')
            path = pathlib.Path(self.directory.name) / 'stdlib.d.tjs'
            path.write_text(text, encoding='utf-8')
            self.standard = self.parse(path.as_uri(), text) or []
            script = pathlib.Path(__file__).with_name('script-builtins.d.tjs')
            if script.exists():
                self.standard += self.parse(script.resolve().as_uri(), script.read_text(encoding='utf-8')) or []
        except (OSError, UnicodeError, subprocess.TimeoutExpired):
            return

    def update(self, sources):
        self.load_standard()
        entries, active = {}, {entry['uri'] for entry in self.standard}
        deadline = time.monotonic() + TIMEOUT
        count = 0
        blocked = set()
        for uri, lexical, text in sources:
            if not is_declaration(uri):
                continue
            remaining = deadline - time.monotonic()
            if count >= MAX_FILES or remaining <= 0:
                blocked.update(symbol['name'] for symbol in lexical)
                continue
            count += 1
            active.add(uri)
            parsed = self.parse(uri, text, remaining)
            if not parsed:
                blocked.update(symbol['name'] for symbol in lexical)
            for entry in parsed or []:
                entries.setdefault(entry['qualified'], []).append(entry)
        # A project declaration replaces the generated declaration of that API.
        project_roots = {name.split('.')[0] for name in entries} | blocked
        entries = {name: values for name, values in entries.items() if name.split('.')[0] not in blocked}
        for entry in self.standard:
            if entry['qualified'].split('.')[0] not in project_roots:
                entries[entry['qualified']] = [entry]
        self.entries = entries
        self.cache = {uri: value for uri, value in self.cache.items() if uri in active}

    def unique(self, name):
        found = self.entries.get(name, [])
        return found[0] if len(found) == 1 else None

    def type_of(self, name):
        entry = self.unique(name)
        if entry is None:
            return None
        symbol = entry['symbol']
        if symbol['kind'] in ('class', 'namespace'):
            return entry['qualified']
        return self.return_type(entry)

    def value_type(self, name):
        entry = self.unique(name)
        if entry is None or entry['symbol']['kind'] in ('function', 'static'):
            return None
        return self.type_of(name)

    def call_type(self, name):
        entry = self.unique(name)
        if entry is None or entry['symbol']['kind'] not in ('function', 'static', 'class'):
            return None
        return self.type_of(name)

    def return_type(self, entry):
        result = entry['symbol']['ret']
        if result in ('', 'any', 'void'):
            return None
        owner = entry['owner']
        while owner:
            local = owner + '.' + result
            candidate = self.unique(local)
            if candidate is not None and candidate['symbol']['kind'] in ('class', 'namespace'):
                return local
            owner = owner.rsplit('.', 1)[0] if '.' in owner else ''
        return result

    def members(self, name, visited=None):
        visited = set() if visited is None else visited
        if name in visited or len(visited) >= 8:
            return {}
        visited.add(name)
        owner = self.unique(name)
        if owner is None:
            return {}
        result = {}
        for base in owner['symbol']['extends']:
            local = owner['owner'] + '.' + base
            result.update(self.members(local if self.unique(local) else base, visited))
        for qualified, candidates in self.entries.items():
            if len(candidates) == 1 and candidates[0]['owner'] == name:
                entry = candidates[0]
                # Constructors are callable through the class, not instance methods.
                if entry['symbol']['name'] != owner['symbol']['name']:
                    result[entry['symbol']['name']] = entry
        return result

    @staticmethod
    def completion(entry):
        node = entry['symbol']
        item = {'label': node['name'], 'kind': {'function': 2, 'static': 2, 'class': 7,
                'namespace': 9, 'property': 10, 'variable': 5}[node['kind']], 'detail': signature(node)}
        if node['doc']:
            item['documentation'] = {'kind': 'plaintext', 'value': node['doc']}
        return item

    @staticmethod
    def location(entry):
        return {'uri': entry['uri'], 'range': entry['range']}

    @staticmethod
    def hover(entry):
        node = entry['symbol']
        return {'contents': {'kind': 'plaintext', 'value': signature(node) + (
            '\n\n' + node['doc'] if node['doc'] else '')}}

HTTP Tutorial
=============

Overview
--------

Unlike most HTTP servers out there zerogw doesn't try to support old CGI
convention of passing environment to your web application. For each
forwarded requests zerogw forwards only a few fields, such as url and
method (configurable) to a backend. This way we produce less internal
traffic and less bloated applications. This also means thay you can't
use most of bloated web frameworks out there. Zerogw is suitable for
fast web applications (and a parts of thereof) which need most out of
machine performance (e.g. you can use it for autocompletion or some
other AJAX). If you want to use big fat web framework there are plenty
of other solutions. If thats ok for you, read on!

.. admonition:: **🚨 Archival Notice**

   This project started as a way to escape the whole Redis licensing drama after March 2024. We wanted to check out alternatives, and Valkey looked like the best option. But by May 2025, `Redis switched back to an open-source license <https://redis.io/blog/agplv3/>`_ from version 8.0 onwards, so although Valkey is a great initiative, this work isn't really needed anymore. Keeping separate VMODs for Redis and Valkey long-term is just too much effort. So unless there's actual interest in the VMOD, I'm planning to **pause development and archive the project** for now.

---

.. image:: https://github.com/carlosabalde/libvmod-valkey/actions/workflows/main.yml/badge.svg?branch=7.7
   :alt: GitHub Actions CI badge
   :target: https://github.com/carlosabalde/libvmod-valkey/actions
.. image:: https://codecov.io/gh/carlosabalde/libvmod-valkey/branch/7.7/graph/badge.svg
   :alt: Codecov badge
   :target: https://codecov.io/gh/carlosabalde/libvmod-valkey

VMOD using the `synchronous libvalkey library API <https://github.com/valkey-io/libvalkey>`_ to access Valkey servers from VCL. Based on the `libvmod-redis VMOD <https://github.com/carlosabalde/libvmod-redis>`_ project.

Highlights:

* **Full support for execution of LUA scripts** (i.e. ``EVAL`` command), including optimistic automatic execution of ``EVALSHA`` commands.
* **All Valkey reply data types are supported**, including partial support to access to components of simple (i.e. not nested) array replies.
* **Valkey pipelines are not (and won't be) supported**. LUA scripting, which is fully supported by the VMOD, it's a much more flexible alternative to pipelines for atomic execution and minimizing latency. Pipelines are hard to use and error prone, specially when using the ``WATCH`` command.
* **Support for classic Valkey deployments** using multiple replicated Valkey servers **and for clustered deployments based on Valkey Cluster**.
* **Support for multiple databases and multiple Valkey connections**, local to each Varnish worker thread, or shared using one or more pools.
* **Support for smart command execution**, selecting the destination server according with the preferred role (i.e. master or slave) and with distance and healthiness metrics collected during execution.
* **Support for Valkey Sentinel**, allowing automatic discovery of sick / healthy servers and changes in their roles.

Looking for official support for this VMOD? Please, contact `Allenta Consulting <https://www.allenta.com>`_, a `Varnish Software Premium partner <https://www.varnish-software.com/partner/allenta-consulting>`_.

SYNOPSIS
========

import valkey;

::

    ##
    ## Subnets.
    ##

    Function subnets(STRING masks="")

    ##
    ## Sentinels.
    ##

    Function sentinels(
        STRING locations="",
        INT period=60,
        INT connection_timeout=500,
        INT command_timeout=0,
        ENUM { RESP2, RESP3, default } protocol="default",
        BOOL tls=false,
        STRING tls_cafile="",
        STRING tls_capath="",
        STRING tls_certfile="",
        STRING tls_keyfile="",
        STRING tls_sni="",
        STRING password="")

    ##
    ## Proxy.
    ##

    # Instance selection.
    Function VOID use(STRING db)

    # Proxied methods.
    Method VOID .add_server(..., STRING db="")
    Function VOID command(..., STRING db="")
    Function VOID timeout(..., STRING db="")
    Function VOID retries(..., STRING db="")
    ...
    Method STRING .stats(..., STRING db="")
    Method INT .counter(..., STRING db="")

    ##
    ## Databases.
    ##

    # Constructor.
    Object db(
        STRING location="",
        ENUM { master, slave, auto, cluster } type="auto",
        INT connection_timeout=1000,
        INT connection_ttl=0,
        INT command_timeout=0,
        INT max_command_retries=0,
        BOOL shared_connections=true,
        INT max_connections=128,
        ENUM { RESP2, RESP3, default } protocol="default",
        BOOL tls=false,
        STRING tls_cafile="",
        STRING tls_capath="",
        STRING tls_certfile="",
        STRING tls_keyfile="",
        STRING tls_sni="",
        STRING user="",
        STRING password="",
        INT sickness_ttl=60,
        BOOL ignore_slaves=false,
        INT max_cluster_hops=32)
    Method VOID .add_server(
        STRING location,
        ENUM { master, slave, auto, cluster } type)

    # Command execution.
    Method VOID .command(STRING name)
    Method VOID .timeout(INT command_timeout)
    Method VOID .retries(INT max_command_retries)
    Method VOID .push(STRING arg)
    Method VOID .execute(BOOL master=true)
    Method VOID .easy_execute(STRING command, [STRING command_args...], BOOL master=true, INT command_timeout, INT max_command_retries)

    # Access to replies.
    Method BOOL .replied()

    Method BOOL .reply_is_error()
    Method BOOL .reply_is_nil()
    Method BOOL .reply_is_status()
    Method BOOL .reply_is_integer()
    Method BOOL .reply_is_boolean()
    Method BOOL .reply_is_double()
    Method BOOL .reply_is_string()
    Method BOOL .reply_is_array()

    Method STRING .get_reply()

    Method STRING .get_error_reply()
    Method STRING .get_status_reply()
    Method INT .get_integer_reply()
    Method BOOL .get_boolean_reply()
    Method REAL .get_double_reply()
    Method STRING .get_string_reply()

    Method INT .get_array_reply_length()
    Method BOOL .array_reply_is_error(INT index)
    Method BOOL .array_reply_is_nil(INT index)
    Method BOOL .array_reply_is_status(INT index)
    Method BOOL .array_reply_is_integer(INT index)
    Method BOOL .array_reply_is_boolean(INT index)
    Method BOOL .array_reply_is_double(INT index)
    Method BOOL .array_reply_is_string(INT index)
    Method BOOL .array_reply_is_array(INT index)
    Method STRING .get_array_reply_value(INT index)

    # Other.
    Method VOID .free()
    Method STRING .stats(
        ENUM { json, prometheus } format="json",
        BOOL stream=0,
        STRING prometheus_name_prefix="vmod_valkey_",
        BOOL prometheus_default_labels=1,
        STRING prometheus_extra_labels="")
    Method INT .counter(STRING name)

EXAMPLES
========

Single server
-------------

::

    sub vcl_init {
        # VMOD configuration: simple case, keeping up to one Valkey connection
        # per Varnish worker thread.
        new db = valkey.db(
            location="192.168.1.100:6379",
            type=master,
            connection_timeout=500,
            shared_connections=false,
            max_connections=1);
    }

    sub vcl_deliver {
        # Simple command execution.
        db.command("SET");
        db.push("foo");
        db.push("Hello world!");
        db.execute();

        # Alternatively, the same can be achieved with one single command
        db.easy_execute("SET", "foo", "Hello world!");

        # LUA scripting.
        db.command("EVAL");
        db.push({"
            server.call('SET', KEYS[1], ARGV[1])
            server.call('SET', KEYS[2], ARGV[1])
        "});
        db.push("2");
        db.push("foo");
        db.push("bar");
        db.push("Atomic hello world!");
        db.execute();

        # Array replies, checking & accessing to reply.
        db.command("MGET");
        db.push("foo");
        db.push("bar");
        db.execute();
        if ((db.reply_is_array()) &&
            (db.get_array_reply_length() == 2)) {
            set resp.http.X-Foo = db.get_array_reply_value(0);
            set resp.http.X-Bar = db.get_array_reply_value(1);
        }
    }

Multiple servers
----------------

::

    sub vcl_init {
        # VMOD configuration: master-slave replication, keeping up to two
        # Valkey connections per Varnish worker thread (up to one to the master
        # server & up to one to the closest slave server).
        valkey.subnets(
            masks={"
                0 192.168.1.102/32,
                1 192.168.1.103/32,
                2 0.0.0.0/32
            "});
        new db = valkey.db(
            location="192.168.1.100:6379",
            type=master,
            connection_timeout=500,
            shared_connections=false,
            max_connections=2);
        db.add_server("192.168.1.101:6379", slave);
        db.add_server("192.168.1.102:6379", slave);
        db.add_server("192.168.1.103:6379", slave);
    }

    sub vcl_deliver {
        # SET submitted to the master server.
        db.command("SET");
        db.push("foo");
        db.push("Hello world!");
        db.execute();

        # GET submitted to one of the slave servers.
        db.command("GET");
        db.push("foo");
        db.execute(false);
        set req.http.X-Foo = db.get_string_reply();
    }

Clustered setup
---------------

::

    sub vcl_init {
        # VMOD configuration: clustered setup, keeping up to 100 Valkey
        # connections per server, all shared between all Varnish worker threads.
        # Two initial cluster servers are provided; remaining servers are
        # automatically discovered.
        new db = valkey.db(
            location="192.168.1.100:6379",
            type=cluster,
            connection_timeout=500,
            shared_connections=true,
            max_connections=128,
            max_cluster_hops=16);
        db.add_server("192.168.1.101:6379", cluster);
    }

    sub vcl_deliver {
        # SET internally routed to the destination server.
        db.command("SET");
        db.push("foo");
        db.push("Hello world!");
        db.execute();

        # GET internally routed to the destination server.
        db.command("GET");
        db.push("foo");
        db.execute(false);
        set req.http.X-Foo = db.get_string_reply();
    }

INSTALLATION
============

The source tree is based on autotools to configure the building, and does also have the necessary bits in place to do functional unit tests using the varnishtest tool.

**Beware this project contains multiples branches (master, 6.0, 7.6, etc.). Please, select the branch to be used depending on your Varnish Cache version (Varnish trunk → master, Varnish 6.0.x → 6.0, Varnish 7.6.x → 7.6, etc.).**

Dependencies:

* `libvalkey <https://github.com/valkey-io/libvalkey>`_ - Valkey client library in C.
* `libev <http://software.schmorp.de/pkg/libev.html>`_ - full-featured and high-performance event loop.

COPYRIGHT
=========

See LICENSE for details.

Public domain implementation of the SHA-1 cryptographic hash function by Steve Reid and embedded in this VMOD (required for the optimistic execution of ``EVALSHA`` commands) has been borrowed from `this project <https://github.com/clibs/sha1/>`_:

* https://github.com/clibs/sha1/blob/master/sha1.c
* https://github.com/clibs/sha1/blob/master/sha1.h

BSD's implementation of the CRC-16 cryptographic hash function by Georges Menie & Salvatore Sanfilippo and embedded in this VMOD (required for the Valkey Cluster slot calculation) has been borrowed from the `Redis project <https://redis.io>`_:

* http://download.redis.io/redis-stable/src/crc16.c

Copyright (c) Carlos Abalde <carlos.abalde@gmail.com>

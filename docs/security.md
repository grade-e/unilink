# Security and Threat Model

## Transport encryption

Plaintext is the default on every transport. The TCP **server** can serve TLS
when the library is built with it; nothing else can.

| | TLS |
|---|---|
| TCP server | Optional, opt-in at build and at runtime |
| TCP client | No |
| UDP, Serial, UDS | No |

### TCP server TLS

Build with `-DWIRESTEAD_ENABLE_TLS=ON`, which requires OpenSSL. A default build
has no OpenSSL dependency and no TLS code in it.

```cpp
auto server = wirestead::tcp_server(9000)
                  .tls("/path/to/fullchain.pem", "/path/to/privkey.pem")
                  .on_data(handler)
                  .build();
```

Both files are PEM. The certificate is a chain file, so intermediates belong in
it. TLS 1.2 is the floor and is not configurable.

Leaving `tls()` unset serves plaintext, which is the default. Setting it in a
build without `WIRESTEAD_ENABLE_TLS` makes `start()` **fail** rather than serve
plaintext: a server asked for encryption that cannot provide it must not come up
looking healthy. An unreadable or malformed certificate fails `start()` the same
way, before the listening socket is bound.

What this does not do: no client certificates, no peer verification, no cipher
suite or ALPN configuration, no certificate reload without a restart. If you need
any of those, terminate TLS outside the library as described below.

### Everything else

The TCP client, UDP, Serial and UDS have no encryption and no hook for adding
it. DTLS is not supported at all - Boost.Asio does not provide it, and UDP's
sessions here are virtual groupings with no place to attach handshake state.

## Intended trust model

`wirestead` is designed for use over networks and channels you already trust:
a local machine, a private LAN, a point-to-point serial/UDS link, or inside
a network perimeter secured by other means (VPN, SSH tunnel, physical
access control). Outside the TCP server TLS described above, it is **not**
designed to be used directly over the public internet or any other untrusted
network, since:

- Data (including any application-level credentials or secrets your
  protocol carries) is visible to anyone who can observe the traffic.
- Data can be modified in transit without detection - there is no message
  authentication.
- `transport::TcpClient`/`transport::TcpServer` and `transport::UdsServer`
  do not authenticate peers beyond what the transport itself provides (a
  TCP handshake, or standard UDS file permissions - `UdsServer` supports
  restricting the socket file's local permissions via
  `UdsServer::socket_permissions(mode)`).

If you need confidentiality, integrity, or peer authentication over an
untrusted network, terminate TLS (or an equivalent) outside `wirestead` -
for example, a reverse proxy/stunnel in front of a TCP server, or an SSH/VPN
tunnel wrapping the connection - and point `wirestead` at the resulting local,
trusted endpoint.

## Reporting a vulnerability

Open an issue on this repository.

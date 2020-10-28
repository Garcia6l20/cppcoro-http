#pragma once

#include <span>

namespace
{
    constexpr std::span<const char> cert{ R"(-----BEGIN CERTIFICATE-----
MIIDrTCCApWgAwIBAgIUU0h7db1NQpAEVvmhDy++pbHt+xgwDQYJKoZIhvcNAQEL
BQAwZjELMAkGA1UEBhMCRlIxDzANBgNVBAgMBkZyYW5jZTERMA8GA1UEBwwIR3Jl
bm9ibGUxEDAOBgNVBAoMB2NwcGNvcm8xDTALBgNVBAsMBGh0dHAxEjAQBgNVBAMM
CWxvY2FsaG9zdDAeFw0yMDEwMjAyMjIwMTRaFw0zMDEwMTgyMjIwMTRaMGYxCzAJ
BgNVBAYTAkZSMQ8wDQYDVQQIDAZGcmFuY2UxETAPBgNVBAcMCEdyZW5vYmxlMRAw
DgYDVQQKDAdjcHBjb3JvMQ0wCwYDVQQLDARodHRwMRIwEAYDVQQDDAlsb2NhbGhv
c3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDFm1IURM62JIyxlT0Y
M7A6wU5ejMWobIKZhps2/B6eHuEnJNSzw2QnVB5+CiTzFuWPu10xPjOV3LQ5Nu79
zS9EkmzPae/KhqifX5MR/2dBRvUXGxd3oCRjH5RCAkbId6VasE8vF+jCU3OO7TyO
R0GlP03IGRySvYIUVvFAbud2NOLrKJpH567STZJnVvxtgbVKDRkoFzok/59myy4i
QSOHmWblXL4FRX003J4Qj/Of/0TI1OVj76w2ywUz1TmKvrxiFuCY8W/InLLDg69M
R7AX/dAi/2e/SJVtDSlvEXVgAm2RKVTTZP7ICTUgUOOshn+1Xn1QvSS6wDM3LN3M
RTrVAgMBAAGjUzBRMB0GA1UdDgQWBBSALkxmDAkTyv3foR1FY03mCWc2bTAfBgNV
HSMEGDAWgBSALkxmDAkTyv3foR1FY03mCWc2bTAPBgNVHRMBAf8EBTADAQH/MA0G
CSqGSIb3DQEBCwUAA4IBAQAlGzSMAYepJNJtZtfT8pq7LeGoPf7zLILxdvM9arIN
g0SfRp0Oygp6cwZmF5g9Uucw3td54KxQfAmo1UAiEczwmT5jighxCLcQk/GqKwvQ
c6hvPOZuf5ehizhM0zck0n+UkIhFA8bs7nSK/JBr8FpUMMRHEXZXVfeTz8OmVASg
d0/C7RZoWY857EYNBCvUDgTW2nDBi3GpgJmx/sAObsP0S7Sg0nyrg+0VTG/C6g/T
NDg6g2tvRw5xYttxdf6O5ZNpuJ2XaLvCOtAEPx6SSjXq3JhhnjKIbgeEylJHT9qi
gkOnjiJLVNGKxwAFEdtuNnZfnd18RkYNbagJmJVdhpDv
-----END CERTIFICATE-----)" };

    constexpr std::span<const char> key{ R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDFm1IURM62JIyx
lT0YM7A6wU5ejMWobIKZhps2/B6eHuEnJNSzw2QnVB5+CiTzFuWPu10xPjOV3LQ5
Nu79zS9EkmzPae/KhqifX5MR/2dBRvUXGxd3oCRjH5RCAkbId6VasE8vF+jCU3OO
7TyOR0GlP03IGRySvYIUVvFAbud2NOLrKJpH567STZJnVvxtgbVKDRkoFzok/59m
yy4iQSOHmWblXL4FRX003J4Qj/Of/0TI1OVj76w2ywUz1TmKvrxiFuCY8W/InLLD
g69MR7AX/dAi/2e/SJVtDSlvEXVgAm2RKVTTZP7ICTUgUOOshn+1Xn1QvSS6wDM3
LN3MRTrVAgMBAAECggEARZHOTuZ+pC+v/OFe1gN0murtjWogOJCjVivGv4/5s4+J
kz9rRzKWMyZxacxmf9Li1TyQrcKJZMyEAtStRVuUtZ6bglZ4nqPT//AlFiQGFTxH
E3BtTadqyB8ZEjg89VzyMUB7UEgpoSjCOWKafDjoCqaD2tvEbIEdp82IODgTc1DC
1T/fUsBUxvRXauyzvjtG8427rg3Zqc+1GC4bK/zOrc5/nX7N2SNhRTQVlpkY/3fO
8aY5Xe2BDkctySw8FXotOTrg2J99mefA1XEcFmTTIWLf+9rkbK08iW05jRHkS81c
LY30JpGYDGLJkY/uI2uOVYVwg9lYpMu50MXmjZPRAQKBgQDn6JyAhw4OXgrnL5U1
kF0kEyBAWBnstyd3mk7QQKCLf8vKtursOlpxBFokN7hd1KcusgAcXjvKDgeKl96T
iEfx+iHLTM3bc37C/NMWrqud4Ui2Bw/URqPeHDXTOqJ0i6+YUr/QUDK+ychEiXbW
IGc5BLkOASfRUEL0BYttGjKHQQKBgQDaInuKSQO0gAJKTFCnVPtpvVjmkqkJkxCQ
ifTmuo+wPpm5YlURZvVzTU8V0Ombtn+R0WgS+GD1O/0vOXLj0C13NaleWpeiB9tj
8N/d9IfT4N/vwCV+589oehWxLq5uOHY0a2begADycuv2RvjEjyHi4GcOnOKe6tiC
ZF9Jck4ClQKBgCfPJF5j0KPivNhmsKRbPcHdjqG8/eZGon3DfVf+YBDSRTdtIvKe
KbxQ1PB2qC1jPbekUqSMAJN0yRKfc1O8By2glICDlKrhLpdmMw4nucoGTCcDG1KD
NcoA6bRy0kRTXjc1rTujKLLbjIdHWaD0OwPsrZ+bzyv9LSEaeo6l+pqBAoGBAMJz
9tlBWXIwotoEyelBEpYiWvvARbvpQ9z1fkGokaq1Q2hFRjwrIidVBWkXQQi0WWht
2m7+x8AVaBAPEGIRFFaumXspGv8wLd0bvxUnhWXVkwswqLxGfVhPbML0MD7FSmpU
S/GQ/kcjN0Hl5qGiTrzm+jfGlya/h55FR8Q7h1s5AoGAGH3mov87GPiwWE0HrYAQ
pasNCW9/QeFRxBLdgaPBqRXv0iT5tEFO4Sd1KiJ/fP0v/lYY5sMMHuWPx/1Mp6uZ
PXjRUbCEKdpOZsroF6rDkCleO5SZzC5fMgmQOFRKG/JPXO4z0sqfFIstIWkb7suq
x5upWG2/c9eFDUONeuUWYt4=
-----END PRIVATE KEY-----)" };
}  // namespace

#include <cppcoro/net/ssl/socket.hpp>

struct ipv4_ssl_server_provider
{
    auto create_listening_sock(cppcoro::io_service& ios) const
    {
        return cppcoro::net::socket::create_tcpv4(ios);
    }
    cppcoro::net::ssl::socket create_connection_sock(cppcoro::io_service& ios) const
    {
        auto sock = cppcoro::net::ssl::socket::create_server(
            ios, cppcoro::net::ssl::certificate{ cert }, cppcoro::net::ssl::private_key{ key });
        sock.host_name("localhost");
        return sock;
    }
};

struct ipv4_ssl_client_provider
{
    auto create_listening_sock(cppcoro::io_service& ios) const
    {
        return cppcoro::net::socket::create_tcpv4(ios);
    }
    cppcoro::net::ssl::socket create_connection_sock(cppcoro::io_service& ios) const
    {
        auto sock = cppcoro::net::ssl::socket::create_client(ios);
        sock.set_peer_verify_mode(cppcoro::net::ssl::peer_verify_mode::required);
        sock.set_verify_flags(cppcoro::net::ssl::verify_flags::allow_untrusted);
        return sock;
    }
};
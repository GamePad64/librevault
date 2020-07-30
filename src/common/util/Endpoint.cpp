#include "Endpoint.h"

#include <QRegularExpression>
#include <QtEndian>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/ip.h>
#include <sys/socket.h>
#endif

namespace librevault {

namespace {
QRegularExpression endpoint_regexp(R"(^\[?([[:xdigit:].:]{2,45})\]?:(\d{1,5})$)");
}

Endpoint::Endpoint(const QHostAddress& addr, quint16 port) : addr(addr), port(port) {}
Endpoint::Endpoint(const QString& addr, quint16 port) : Endpoint(QHostAddress(addr), port) {}

Endpoint Endpoint::fromString(const QString& str) {
  auto match = endpoint_regexp.match(str);
  if (!match.hasMatch()) throw EndpointNotMatched();

  return {match.captured(1), match.captured(2).toUShort()};
}

QString Endpoint::toString() const {
  QHostAddress addr_converted = addr;
  {
    bool ok = false;
    quint32 addr4 = addr_converted.toIPv4Address(&ok);
    if (ok) addr_converted = QHostAddress(addr4);
  }

  switch (addr_converted.protocol()) {
    case QAbstractSocket::IPv4Protocol:
      return addr_converted.toString() + ":" + QString::number(port);
    case QAbstractSocket::IPv6Protocol:
      return QString("[%1]").arg(addr_converted.toString()) + ":" + QString::number(port);
    default:
      return {};
  }
}

Endpoint Endpoint::fromPacked4(QByteArray packed) {
  packed = packed.leftJustified(6, 0, true);

  Endpoint endpoint;
  endpoint.addr = QHostAddress(qFromBigEndian(*reinterpret_cast<quint32*>(packed.mid(0, 4).data())));
  endpoint.port = qFromBigEndian(*reinterpret_cast<quint16*>(packed.mid(4, 2).data()));
  return endpoint;
}

Endpoint Endpoint::fromPacked6(QByteArray packed) {
  packed = packed.leftJustified(18, 0, true);

  Endpoint endpoint;
  endpoint.addr = QHostAddress(reinterpret_cast<quint8*>(packed.mid(0, 16).data()));
  endpoint.port = qFromBigEndian(*reinterpret_cast<quint16*>(packed.mid(16, 2).data()));
  return endpoint;
}

EndpointList Endpoint::fromPackedList4(const QByteArray& packed) {
  EndpointList l;
  l.reserve(packed.size() / 6);
  for (int i = 0; i < packed.size(); i += 6) l += fromPacked4(packed.mid(i, 6));
  return l;
}

EndpointList Endpoint::fromPackedList6(const QByteArray& packed) {
  EndpointList l;
  l.reserve(packed.size() / 18);
  for (int i = 0; i < packed.size(); i += 18) l += fromPacked6(packed.mid(i, 18));
  return l;
}

Endpoint Endpoint::fromSockaddr(const sockaddr& sa) {
  quint16 port_be = 0;
  switch (sa.sa_family) {
    case AF_INET:
      port_be = reinterpret_cast<const sockaddr_in*>(&sa)->sin_port;
      break;
    case AF_INET6:
      port_be = reinterpret_cast<const sockaddr_in6*>(&sa)->sin6_port;
      break;
    default:
      throw InvalidAddressFamily();
  }
  return {QHostAddress(&sa), qFromBigEndian(port_be)};
}

std::tuple<sockaddr_storage, size_t> Endpoint::toSockaddr() const {
  sockaddr_storage sa = {};

  switch (addr.protocol()) {
    case QAbstractSocket::IPv4Protocol: {
      auto sa4 = (sockaddr_in*)&sa;
      sa4->sin_family = AF_INET;
      sa4->sin_port = qToBigEndian(port);
      sa4->sin_addr.s_addr = addr.toIPv4Address();
      return {sa, sizeof(sockaddr_in)};
    }
    case QAbstractSocket::IPv6Protocol: {
      auto sa6 = (sockaddr_in6*)&sa;
      sa6->sin6_family = AF_INET6;
      sa6->sin6_port = qToBigEndian(port);
      Q_IPV6ADDR addr6 = addr.toIPv6Address();
      std::copy((const char*)&addr6, (const char*)&addr6 + 16, (char*)&sa6->sin6_addr);
      return {sa, sizeof(sockaddr_in6)};
    }
    default:
      return {sa, sizeof(sockaddr_storage)};
  }
}

Endpoint Endpoint::fromJson(const QJsonObject& j) { return {j["ip"].toString(), quint16(j["port"].toInt())}; }

QJsonObject Endpoint::toJson() const { return {{"ip", addr.toString()}, {"port", port}}; }

QDebug operator<<(QDebug debug, const Endpoint& endpoint) {
  QDebugStateSaver saver(debug);
  debug.nospace() << "Endpoint(" << endpoint.toString() << ")";
  return debug;
}

}  // namespace librevault
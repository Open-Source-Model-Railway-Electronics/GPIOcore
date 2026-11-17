#include "webremote.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

// ── The phone page (English / Dutch by the phone's language) ───────────────────
static const char kPage[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GPIOcore remote</title>
<style>
:root{--bg:#f4f6f9;--card:#fff;--fg:#1f2937;--muted:#64748b;--line:#e2e8f0;--blue:#2563eb;--red:#dc2626}
@media (prefers-color-scheme:dark){:root{--bg:#0f172a;--card:#1e293b;--fg:#e2e8f0;--muted:#94a3b8;--line:#334155}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif;padding:12px}
h1{font-size:20px;margin:4px 0 2px}.sub{color:var(--muted);font-size:13px;margin-bottom:12px}
.list{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:12px}
.list button{flex:1 1 45%;padding:12px;border-radius:10px;border:2px solid var(--line);background:var(--card);color:var(--fg);font-size:15px;text-align:left}
.list button.sel{border-color:var(--blue)}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px}
.pos{border-top:1px solid var(--line);padding-top:10px;margin-top:10px}
.pos .head{display:flex;justify-content:space-between;align-items:baseline}
.pos .name{font-weight:600}.pos .val{font-size:30px;font-weight:700}
input[type=range]{width:100%;height:44px}
.row{display:flex;gap:8px;margin-top:6px}.row button{flex:1;padding:12px 6px;border-radius:10px;font-size:15px;font-weight:600;border:2px solid var(--line);background:transparent;color:var(--fg)}
.go{color:#fff!important;border:none!important}.c{background:var(--red)}.s{background:var(--blue)}
.small{color:var(--muted);font-size:13px;text-align:center;margin-top:10px}.err{color:var(--red);text-align:center;margin:8px}
</style></head><body>
<h1>GPIOcore</h1><div class="sub" id="board"></div>
<div class="list" id="list"></div>
<div class="card" id="card" style="display:none">
 <div id="title" style="font-weight:600"></div>
 <div class="row"><button class="go c" id="goC"></button><button class="go s" id="goS"></button></div>
 <div class="pos"><div class="head"><span class="name" id="nC"></span><span class="val"><span id="vC">90</span>&deg;</span></div>
  <input type="range" id="sC" min="10" max="170" value="90">
  <div class="row"><button id="mC">&minus;1&deg;</button><button id="pC">+1&deg;</button></div></div>
 <div class="pos"><div class="head"><span class="name" id="nS"></span><span class="val"><span id="vS">90</span>&deg;</span></div>
  <input type="range" id="sS" min="10" max="170" value="90">
  <div class="row"><button id="mS">&minus;1&deg;</button><button id="pS">+1&deg;</button></div></div>
 <div class="small" id="info"></div>
</div>
<div class="err" id="err"></div>
<script>
const nl=(navigator.language||'').startsWith('nl');
const T=nl?{goC:'◀ Afbuigend',goS:'Rechtdoor ▶',nC:'Stand afbuigend',nS:'Stand rechtdoor',
 hint:'Schuif: de servo beweegt mee en de stand wordt meteen opgeslagen.',none:'Geen servo’s ingesteld.',off:'Geen board verbonden in de GUI.'}
 :{goC:'◀ Curved',goS:'Straight ▶',nC:'Curved position',nS:'Straight position',
 hint:'Slide: the servo follows and the position is stored at once.',none:'No servos set up.',off:'No board connected in the GUI.'};
const tok=new URLSearchParams(location.search).get('t');let st=null,sel=-1,touched=0;
const $=id=>document.getElementById(id);
['goC','goS','nC','nS'].forEach(k=>$(k).textContent=T[k]);$('info').textContent=T.hint;
async function api(path,args){const q=new URLSearchParams(Object.assign({t:tok},args||{}));
 const r=await fetch(path+'?'+q,{method:path=='/api/state'?'GET':'POST',cache:'no-store'});if(!r.ok)throw new Error(r.status);return path=='/api/state'?r.json():null}
function cur(){return st&&st.servos.find(s=>s.index==sel)}
function setPos(w,v){$(w?'sS':'sC').value=v;$(w?'vS':'vC').textContent=v}
function draw(){$('board').textContent=(st.board||'')+(st.connected?'':' · '+T.off);
 const l=$('list');l.innerHTML='';if(!st.servos.length){l.textContent=T.none}
 st.servos.forEach(s=>{const b=document.createElement('button');b.className=s.index==sel?'sel':'';
  b.innerHTML='<b>'+(s.name||('Out '+(s.index+1)))+'</b><br><small>'+s.min+'° / '+s.max+'°</small>';
  b.onclick=()=>{sel=s.index;touched=0;draw()};l.appendChild(b)});
 const s=cur();$('card').style.display=s?'block':'none';
 if(s){$('title').textContent=s.name||('Out '+(s.index+1));
  if(Date.now()-touched>2000){setPos(0,s.min);setPos(1,s.max)}}}   // do not jump while you are sliding
async function load(){try{st=await api('/api/state');$('err').textContent='';draw()}catch(e){$('err').textContent='⚠ '+e.message}}
// sliding: at most 4 updates per second, the newest value always goes out
const pend=[null,null];let busy=false;
function adjust(w,v){touched=Date.now();$(w?'vS':'vC').textContent=v;pend[w]=v;if(!busy)flush()}
async function flush(){const w=pend[0]!==null?0:pend[1]!==null?1:-1;if(w<0){busy=false;return}
 busy=true;const v=pend[w];pend[w]=null;try{await api('/api/adjust',{o:sel,w:w,a:v})}catch(e){}setTimeout(flush,250)}
$('sC').oninput=e=>adjust(0,+e.target.value);$('sS').oninput=e=>adjust(1,+e.target.value);
function step(w,d){const sl=$(w?'sS':'sC');const v=Math.min(170,Math.max(10,+sl.value+d));sl.value=v;adjust(w,v)}
$('mC').onclick=()=>step(0,-1);$('pC').onclick=()=>step(0,1);$('mS').onclick=()=>step(1,-1);$('pS').onclick=()=>step(1,1);
async function go(s){if(!cur())return;touched=0;await api('/api/move',{o:sel,s:s})}
$('goC').onclick=()=>go(0);$('goS').onclick=()=>go(1);
load();setInterval(load,3000);
</script></body></html>)HTML";

WebRemote::WebRemote(QObject *parent) : QObject(parent) {}

bool WebRemote::start(quint16 port)
{
    stop();
    m_server = new QTcpServer(this);
    for (quint16 p = port; p < port + 10; ++p)
        if (m_server->listen(QHostAddress::AnyIPv4, p))
        {
            static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
            m_token.clear();
            for (int i = 0; i < 6; ++i)
                m_token += QChar(alphabet[QRandomGenerator::global()->bounded(int(sizeof(alphabet) - 1))]);
            connect(m_server, &QTcpServer::newConnection, this, &WebRemote::onConnection);
            return true;
        }
    delete m_server;
    m_server = nullptr;
    return false;
}

void WebRemote::stop()
{
    if (!m_server) return;
    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
}

bool    WebRemote::isRunning() const { return m_server && m_server->isListening(); }
quint16 WebRemote::port() const      { return isRunning() ? m_server->serverPort() : 0; }

QString WebRemote::url() const
{
    if (!isRunning()) return QString();
    return QStringLiteral("http://%1:%2/?t=%3").arg(lanAddress()).arg(port()).arg(m_token);
}

QString lanAddress()
{
    // prefer a private IPv4 address of an interface that is up (192.168.x, 10.x, 172.16-31.x)
    QString fallback;
    for (const QNetworkInterface &nif : QNetworkInterface::allInterfaces())
    {
        if (!(nif.flags() & QNetworkInterface::IsUp) || !(nif.flags() & QNetworkInterface::IsRunning)
            || (nif.flags() & QNetworkInterface::IsLoopBack))
            continue;
        const QString name = nif.humanReadableName().toLower();
        const bool virt = name.contains(QLatin1String("virtual")) || name.contains(QLatin1String("vethernet"))
                       || name.contains(QLatin1String("vmware")) || name.contains(QLatin1String("vbox"));
        for (const QNetworkAddressEntry &e : nif.addressEntries())
        {
            const QHostAddress a = e.ip();
            if (a.protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const quint32 v = a.toIPv4Address();
            const bool priv = (v >> 24) == 10 || (v >> 16) == 0xC0A8 || ((v >> 20) == 0xAC1);
            if (priv && !virt)
                return a.toString();
            if (priv && fallback.isEmpty())
                fallback = a.toString();
        }
    }
    return fallback.isEmpty() ? QStringLiteral("127.0.0.1") : fallback;
}

void WebRemote::onConnection()
{
    while (QTcpSocket *s = m_server->nextPendingConnection())
    {
        connect(s, &QTcpSocket::readyRead, this, [this, s] { handle(s); });
        connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    }
}

void WebRemote::reply(QTcpSocket *s, int status, const QByteArray &type, const QByteArray &body)
{
    const QByteArray text = status == 200 ? "OK" : status == 403 ? "Forbidden" : "Not Found";
    s->write("HTTP/1.1 " + QByteArray::number(status) + ' ' + text + "\r\n"
             "Content-Type: " + type + "\r\n"
             "Cache-Control: no-store\r\n"
             "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
             "Connection: close\r\n\r\n" + body);
    s->disconnectFromHost();
}

void WebRemote::handle(QTcpSocket *s)
{
    // only the request line matters: METHOD /path?query HTTP/1.1
    if (!s->canReadLine())
        return;
    const QList<QByteArray> parts = s->readLine().trimmed().split(' ');
    s->readAll();                         // headers are not needed
    if (parts.size() < 2)
        return reply(s, 404, "text/plain", "bad request");
    const QUrl url(QString::fromLatin1(parts[1]));
    const QUrlQuery q(url);
    if (q.queryItemValue(QStringLiteral("t")) != m_token)
        return reply(s, 403, "text/plain; charset=utf-8",
                     "Open the address shown in the GPIOcore program (it contains a code).");
    const QString path = url.path();
    const int o = q.queryItemValue(QStringLiteral("o")).toInt();
    if (path == QLatin1String("/"))
        return reply(s, 200, "text/html; charset=utf-8", QByteArray(kPage));
    if (path == QLatin1String("/api/state"))
        return reply(s, 200, "application/json", QJsonDocument(stateProvider ? stateProvider() : QJsonObject())
                                                      .toJson(QJsonDocument::Compact));
    if (path == QLatin1String("/api/live"))
        emit liveRequested(o, qBound(10, q.queryItemValue(QStringLiteral("a")).toInt(), 170));
    else if (path == QLatin1String("/api/store"))
        emit storeRequested(o, q.queryItemValue(QStringLiteral("w")).toInt() ? 1 : 0,
                            qBound(10, q.queryItemValue(QStringLiteral("a")).toInt(), 170));
    else if (path == QLatin1String("/api/adjust"))
        emit adjustRequested(o, q.queryItemValue(QStringLiteral("w")).toInt() ? 1 : 0,
                             qBound(10, q.queryItemValue(QStringLiteral("a")).toInt(), 170));
    else if (path == QLatin1String("/api/move"))
        emit moveRequested(o, q.queryItemValue(QStringLiteral("s")).toInt() ? 1 : 0);
    else
        return reply(s, 404, "text/plain", "not found");
    reply(s, 200, "text/plain", "ok");
}

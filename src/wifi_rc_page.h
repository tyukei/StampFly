// StampFly Wi-Fi RC — embedded control page (served at http://192.168.4.1/).
//
// Fully self-contained: no CDN, no external assets. This matters because while
// the phone is joined to the drone's soft-AP there is NO internet access, so
// everything (CSS, JS, virtual joysticks) must be inlined here.
//
// Stored in PROGMEM to keep it out of RAM.
#pragma once
#include <Arduino.h>

static const char WIFI_RC_PAGE[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0e14">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>StampFly RC</title>
<style>
  :root{
    --bg:#0b0e14; --panel:#131824; --panel2:#1b2130; --line:#273043;
    --txt:#e6edf6; --dim:#8a97ad; --accent:#37d0a8; --warn:#ffb020;
    --danger:#ff4d5e; --arm:#ff4d5e; --ok:#37d0a8;
    --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  }
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  html,body{margin:0;height:100%;background:var(--bg);color:var(--txt);
    font-family:system-ui,-apple-system,"Hiragino Kaku Gothic ProN","Yu Gothic",sans-serif;
    overscroll-behavior:none;user-select:none}
  body{display:flex;flex-direction:column;padding:env(safe-area-inset-top) 10px 10px;gap:10px}
  header{display:flex;align-items:center;gap:10px;padding-top:8px}
  header h1{font-size:15px;margin:0;letter-spacing:.06em;font-weight:600;flex:1}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--danger);
    box-shadow:0 0 8px currentColor;transition:.2s}
  .dot.on{background:var(--ok)}
  #conn{font-size:12px;color:var(--dim);font-family:var(--mono)}
  .row{display:flex;gap:10px}
  .card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:12px}
  .seg{display:flex;background:var(--panel2);border:1px solid var(--line);border-radius:11px;overflow:hidden}
  .seg button{flex:1;background:none;border:0;color:var(--dim);padding:9px 0;font-size:13px;font-weight:600}
  .seg button.sel{background:var(--accent);color:#04110d}
  /* Sensors */
  #sensors{flex:0 0 auto}
  .sgrid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px 10px}
  .s{background:var(--panel2);border-radius:10px;padding:7px 9px}
  .s .k{font-size:10px;color:var(--dim);letter-spacing:.08em;text-transform:uppercase}
  .s .v{font-size:16px;font-family:var(--mono);font-variant-numeric:tabular-nums}
  .att{display:flex;gap:10px;margin-top:9px}
  .bar{flex:1;background:var(--panel2);border-radius:8px;height:26px;position:relative;overflow:hidden}
  .bar b{position:absolute;top:0;bottom:0;left:50%;width:2px;background:var(--accent)}
  .bar i{position:absolute;top:3px;bottom:3px;left:50%;width:0;background:var(--accent);
    opacity:.35;border-radius:4px}
  .bar span{position:absolute;left:8px;top:5px;font-size:11px;color:var(--dim)}
  .bar em{position:absolute;right:8px;top:4px;font-size:13px;font-family:var(--mono);font-style:normal}
  /* Control area */
  #control{flex:1 1 auto;display:flex;flex-direction:column;gap:10px;min-height:0}
  .pads{flex:1 1 auto;display:flex;gap:10px;min-height:220px}
  .pad{flex:1;background:var(--panel);border:1px solid var(--line);border-radius:16px;
    position:relative;touch-action:none;overflow:hidden}
  .pad .lbl{position:absolute;top:8px;left:0;right:0;text-align:center;font-size:11px;color:var(--dim)}
  .pad .lblx{position:absolute;bottom:8px;left:0;right:0;text-align:center;font-size:11px;color:var(--dim)}
  .knob{position:absolute;width:64px;height:64px;margin:-32px 0 0 -32px;border-radius:50%;
    background:radial-gradient(circle at 35% 30%,#4a5875,#222a3b);border:2px solid var(--accent);
    box-shadow:0 4px 16px rgba(0,0,0,.5)}
  .cross{position:absolute;inset:0;pointer-events:none}
  .cross:before,.cross:after{content:"";position:absolute;background:var(--line)}
  .cross:before{left:50%;top:12%;bottom:12%;width:1px;transform:translateX(-.5px)}
  .cross:after{top:50%;left:12%;right:12%;height:1px;transform:translateY(-.5px)}
  /* Motor sliders */
  #motorpad{flex:1 1 auto;display:none;gap:10px}
  #motorpad.show{display:flex}
  .pads.hide{display:none}
  .msl{flex:1;background:var(--panel);border:1px solid var(--line);border-radius:16px;
    display:flex;flex-direction:column;align-items:center;padding:10px 6px;touch-action:none}
  .msl .name{font-size:12px;color:var(--dim);font-weight:600}
  .msl .track{flex:1;width:34px;margin:8px 0;background:var(--panel2);border-radius:17px;
    position:relative;overflow:hidden}
  .msl .fill{position:absolute;left:0;right:0;bottom:0;height:0;
    background:linear-gradient(180deg,var(--accent),#1c8f74);border-radius:17px}
  .msl .pct{font-size:14px;font-family:var(--mono)}
  /* Buttons */
  .btns{display:flex;gap:10px}
  .btn{flex:1;border:0;border-radius:14px;padding:16px 0;font-size:16px;font-weight:700;
    color:#fff;background:var(--panel2);border:1px solid var(--line);letter-spacing:.05em}
  .btn.arm{background:#22303f;border-color:#2f6f5c;color:var(--ok)}
  .btn.arm.armed{background:var(--arm);border-color:var(--arm);color:#fff;animation:pulse 1s infinite}
  .btn.stop{background:#2a1620;border-color:#5a2733;color:var(--danger);flex:0 0 34%}
  @keyframes pulse{50%{filter:brightness(1.25)}}
  .hint{font-size:11px;color:var(--dim);text-align:center;line-height:1.5}
</style>
</head>
<body>
<header>
  <span class="dot" id="dot"></span>
  <h1>StampFly RC</h1>
  <span id="conn">未接続</span>
</header>

<section class="card" id="sensors">
  <div class="sgrid">
    <div class="s"><div class="k">Accel X</div><div class="v" id="ax">--</div></div>
    <div class="s"><div class="k">Accel Y</div><div class="v" id="ay">--</div></div>
    <div class="s"><div class="k">Accel Z</div><div class="v" id="az">--</div></div>
    <div class="s"><div class="k">Gyro X</div><div class="v" id="gx">--</div></div>
    <div class="s"><div class="k">Gyro Y</div><div class="v" id="gy">--</div></div>
    <div class="s"><div class="k">Gyro Z</div><div class="v" id="gz">--</div></div>
  </div>
  <div class="att">
    <div class="bar"><i id="rollf"></i><b></b><span>ROLL</span><em id="roll">0&deg;</em></div>
    <div class="bar"><i id="pitchf"></i><b></b><span>PITCH</span><em id="pitch">0&deg;</em></div>
  </div>
</section>

<div class="seg" id="modeseg">
  <button data-mode="mix" class="sel">飛行モード</button>
  <button data-mode="motor">モーターテスト</button>
</div>

<section id="control">
  <div class="pads" id="pads">
    <div class="pad" id="padL">
      <div class="lbl">スロットル (上下)</div>
      <div class="cross"></div><div class="knob" id="knobL"></div>
      <div class="lblx">ラダー / ヨー (左右)</div>
    </div>
    <div class="pad" id="padR">
      <div class="lbl">エレベーター (上下)</div>
      <div class="cross"></div><div class="knob" id="knobR"></div>
      <div class="lblx">エルロン / ロール (左右)</div>
    </div>
  </div>

  <div id="motorpad">
    <div class="msl" data-m="0"><div class="name">FL</div><div class="track"><div class="fill"></div></div><div class="pct">0%</div></div>
    <div class="msl" data-m="1"><div class="name">FR</div><div class="track"><div class="fill"></div></div><div class="pct">0%</div></div>
    <div class="msl" data-m="2"><div class="name">RL</div><div class="track"><div class="fill"></div></div><div class="pct">0%</div></div>
    <div class="msl" data-m="3"><div class="name">RR</div><div class="track"><div class="fill"></div></div><div class="pct">0%</div></div>
  </div>

  <div class="btns">
    <button class="btn stop" id="stop">STOP</button>
    <button class="btn arm" id="arm">ARM</button>
  </div>
  <div class="hint">初回テストは必ずプロペラを外して行ってください。<br>ARM中に通信が途切れると自動停止します。</div>
</section>

<script>
"use strict";
var mode="mix", armed=false;
var ctl={t:0,r:0,p:0,y:0,m:[0,0,0,0]};   // t 0..1, r/p/y -1..1, m[] 0..1
var ws=null, wantOpen=true;

var $=function(id){return document.getElementById(id)};
function fmt(x,d){return (x>=0?" ":"")+x.toFixed(d==null?2:d)}

/* ---- WebSocket ---- */
function connect(){
  try{ ws=new WebSocket("ws://"+location.hostname+":81/"); }catch(e){ retry(); return; }
  ws.onopen=function(){ $("dot").classList.add("on"); $("conn").textContent="接続中"; };
  ws.onclose=function(){ $("dot").classList.remove("on"); $("conn").textContent="未接続"; armed=false; setArm(); retry(); };
  ws.onerror=function(){ try{ws.close()}catch(e){} };
  ws.onmessage=function(ev){ try{render(JSON.parse(ev.data))}catch(e){} };
}
function retry(){ if(wantOpen) setTimeout(connect,800); }
function send(obj){ if(ws&&ws.readyState===1) ws.send(JSON.stringify(obj)); }
connect();

/* ---- 20Hz control uplink ---- */
setInterval(function(){
  if(mode==="mix") send({arm:armed,mode:"mix",t:+ctl.t.toFixed(3),r:+ctl.r.toFixed(3),p:+ctl.p.toFixed(3),y:+ctl.y.toFixed(3)});
  else send({arm:armed,mode:"motor",m:ctl.m.map(function(v){return +v.toFixed(3)})});
},50);

/* ---- telemetry render ---- */
function render(d){
  if(d.armed!==undefined){ if(d.armed!==armed){armed=d.armed;setArm();} }
  if(d.ax!==undefined){$("ax").textContent=fmt(d.ax);$("ay").textContent=fmt(d.ay);$("az").textContent=fmt(d.az);}
  if(d.gx!==undefined){$("gx").textContent=fmt(d.gx,1);$("gy").textContent=fmt(d.gy,1);$("gz").textContent=fmt(d.gz,1);}
  if(d.roll!==undefined){
    $("roll").innerHTML=Math.round(d.roll)+"&deg;"; $("pitch").innerHTML=Math.round(d.pitch)+"&deg;";
    attBar("rollf",d.roll); attBar("pitchf",d.pitch);
  }
}
function attBar(id,deg){
  var f=$(id), w=Math.min(Math.abs(deg)/45,1)*50;
  if(deg>=0){f.style.left="50%";f.style.right="auto";}else{f.style.right="50%";f.style.left="auto";}
  f.style.width=w+"%";
}

/* ---- ARM / STOP ---- */
function setArm(){ var b=$("arm"); b.classList.toggle("armed",armed); b.textContent=armed?"DISARM":"ARM"; }
$("arm").addEventListener("click",function(){ armed=!armed; setArm(); send({arm:armed,mode:mode}); });
$("stop").addEventListener("click",function(){
  armed=false; ctl.t=0; ctl.r=ctl.p=ctl.y=0; ctl.m=[0,0,0,0];
  resetKnobs(); resetSliders(); setArm(); send({stop:true});
});

/* ---- mode switch ---- */
Array.prototype.forEach.call($("modeseg").children,function(btn){
  btn.addEventListener("click",function(){
    mode=btn.dataset.mode;
    Array.prototype.forEach.call($("modeseg").children,function(b){b.classList.toggle("sel",b===btn)});
    var m=mode==="motor";
    $("pads").classList.toggle("hide",m);
    $("motorpad").classList.toggle("show",m);
    ctl.t=0;ctl.r=ctl.p=ctl.y=0;ctl.m=[0,0,0,0]; resetKnobs(); resetSliders();
  });
});

/* ---- virtual joysticks ---- */
function makePad(padId,knobId,onMove,selfCenterX,selfCenterY){
  var pad=$(padId),knob=$(knobId),active=null;
  function place(nx,ny){ // nx,ny in -1..1
    var r=pad.getBoundingClientRect();
    knob.style.left=(50+nx*38)+"%"; knob.style.top=(50-ny*38)+"%";
  }
  function reset(){ knob.__reset&&0; if(selfCenterX)cx=0; if(selfCenterY)cy=0; place(cx,cy); onMove(cx,cy); }
  var cx=0,cy=0;
  knob.__center=function(){ if(selfCenterX)cx=0; if(selfCenterY)cy=0; place(cx,cy); onMove(cx,cy); };
  function pt(e){ return e.touches?e.touches[0]:e; }
  function move(e){
    if(active===null)return; e.preventDefault();
    var r=pad.getBoundingClientRect(), p=pt(e);
    var nx=((p.clientX-r.left)/r.width-0.5)/0.38;
    var ny=(0.5-(p.clientY-r.top)/r.height)/0.38;
    nx=Math.max(-1,Math.min(1,nx)); ny=Math.max(-1,Math.min(1,ny));
    cx=nx;cy=ny; place(nx,ny); onMove(nx,ny);
  }
  function up(){ active=null; knob.__center(); }
  pad.addEventListener("touchstart",function(e){active=1;move(e);},{passive:false});
  pad.addEventListener("touchmove",move,{passive:false});
  pad.addEventListener("touchend",up); pad.addEventListener("touchcancel",up);
  pad.addEventListener("mousedown",function(e){active=1;move(e);});
  window.addEventListener("mousemove",function(e){if(active)move(e);});
  window.addEventListener("mouseup",up);
  place(0,0);
  return knob;
}
// Left: throttle (Y, latching, no self-center) + yaw (X, self-center)
var knobL=makePad("padL","knobL",function(nx,ny){
  ctl.y=nx;                 // yaw
  ctl.t=(ny+1)/2;           // throttle 0..1 (down=0, up=1)
},true,false);
// Right: pitch (Y) + roll (X), both self-center
var knobR=makePad("padR","knobR",function(nx,ny){
  ctl.r=nx; ctl.p=ny;
},true,true);
function resetKnobs(){ if(knobL.__center)knobL.__center(); if(knobR.__center)knobR.__center(); }

/* ---- motor sliders ---- */
function makeSlider(el){
  var m=+el.dataset.m, track=el.querySelector(".track"), fill=el.querySelector(".fill"), pct=el.querySelector(".pct"), active=false;
  function set(v){ v=Math.max(0,Math.min(1,v)); ctl.m[m]=v; fill.style.height=(v*100)+"%"; pct.textContent=Math.round(v*100)+"%"; }
  function at(e){ var p=e.touches?e.touches[0]:e, r=track.getBoundingClientRect();
    set(1-(p.clientY-r.top)/r.height); }
  el.addEventListener("touchstart",function(e){active=true;at(e);e.preventDefault();},{passive:false});
  el.addEventListener("touchmove",function(e){if(active){at(e);e.preventDefault();}},{passive:false});
  el.addEventListener("touchend",function(){active=false;});
  el.addEventListener("mousedown",function(e){active=true;at(e);});
  window.addEventListener("mousemove",function(e){if(active)at(e);});
  window.addEventListener("mouseup",function(){active=false;});
  el.__set=set;
}
var sliders=Array.prototype.slice.call(document.querySelectorAll(".msl"));
sliders.forEach(makeSlider);
function resetSliders(){ sliders.forEach(function(el){el.__set(0)}); }

setArm();
</script>
</body>
</html>
)HTMLPAGE";

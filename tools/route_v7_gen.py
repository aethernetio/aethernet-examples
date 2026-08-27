from pathlib import Path
import html

H=Path('candidate-a.html').read_text(); J=Path('network-map-candidate-a.js').read_text(); W=Path('network-map-webgl-candidate-a.js').read_text()
V={
 'interactive-soft':('Interactive Context · Soft Coast',3.4,.13,1.45,.50),
 'interactive-crisp':('Interactive Context · Crisp Coast',2.6,.08,1.35,.54),
}

def one(s,a,b,n):
 c=s.count(a)
 if c!=1: raise RuntimeError(f'{n}: {c}')
 return s.replace(a,b,1)

def region(s,a,b,r,n):
 i=s.find(a); j=s.find(b,i)
 if i<0 or j<0: raise RuntimeError(n)
 return s[:i]+r+s[j:]

ROUTE="""    const buildRouteVectors = (from,to,nearest) => {
        const dot=clamp(from.x*to.x+from.y*to.y+from.z*to.z,-1,1);
        const angularDistance=Math.acos(dot);
        if(angularDistance<=USER_REGION_ANGULAR_RADIUS*1.005){return [];}
        const startProgress=clamp(USER_REGION_ANGULAR_RADIUS/angularDistance,0,.985);
        const remaining=Math.max(0,angularDistance-USER_REGION_ANGULAR_RADIUS);
        const factor=clamp(remaining/(32*Math.PI/180),0,1);
        const liftAmplitude=nearest?.004+factor*.106:.003+factor*.071;
        const vectors=new Array(ROUTE_STEPS+1);
        for(let index=0;index<=ROUTE_STEPS;index++){
            const rp=index/ROUTE_STEPS;
            const p=startProgress+(1-startProgress)*rp;
            const surface=slerp(from,to,p);
            const lift=1+Math.sin(Math.PI*rp)*liftAmplitude;
            vectors[index]={x:surface.x*lift,y:surface.y*lift,z:surface.z*lift};
        }
        return vectors;
    };



"""

STATE="""    let hasManualInteraction = false;
    let cameraLockedByUser=false;
    let lockedFocusLocation=null;
    let selectedServerLocation=null;
    let manualZoomScale=1;
    let nodeHitTargets=[];
    let pinchStartDistance=0;
    let pinchStartZoomScale=1;
    const MIN_MANUAL_ZOOM=.50;
    const MAX_MANUAL_ZOOM=3.40;
"""

CONTROL_JS="""
    const lockCameraToCurrentFocus=()=>{cameraLockedByUser=true;hasManualInteraction=true;if(!lockedFocusLocation){lockedFocusLocation=selectedServerLocation||scene.nearest?.location||null;}};
    const applyManualZoom=(next,pivotX,pivotY)=>{const z=clamp(next,MIN_MANUAL_ZOOM,MAX_MANUAL_ZOOM);if(Math.abs(z-manualZoomScale)<.0001)return;lockCameraToCurrentFocus();const ratio=z/Math.max(manualZoomScale,.0001);manualZoomScale=z;const f=getFocusRegion(),x=Number.isFinite(pivotX)?pivotX:f.centerX,y=Number.isFinite(pivotY)?pivotY:f.centerY;globeCenterX=x-(x-globeCenterX)*ratio;globeCenterY=y-(y-globeCenterY)*ratio;globeRadius*=ratio;globeLayoutTransition=null;rotationVelocity=0;pitchVelocity=0;lastGpuFrameState='';watermarkMaskState='';};
    const focusOnServerLocation=location=>{if(!scene.userLocation||!location)return;selectedServerLocation=location;lockedFocusLocation=location;cameraLockedByUser=true;hasManualInteraction=true;manualZoomScale=1.12;const o=orientationForLocations([scene.userLocation,location]);baseRotation=targetBaseRotation=o.rotation;basePitch=targetBasePitch=viewPitch=o.pitch;manualRotation=targetManualRotation=manualPitch=targetManualPitch=rotationVelocity=pitchVelocity=focusHoldUntil=0;beginGlobeLayoutTransition(targetGlobeLayout(getFocusRegion(),baseRotation));lastGpuFrameState='';};
    const fitFastest=()=>{if(!scene.userLocation||!scene.nearest?.location)return;selectedServerLocation=lockedFocusLocation=null;cameraLockedByUser=false;hasManualInteraction=false;manualZoomScale=1;const o=orientationForLocations([scene.userLocation,scene.nearest.location]);baseRotation=targetBaseRotation=o.rotation;basePitch=targetBasePitch=viewPitch=o.pitch;manualRotation=targetManualRotation=manualPitch=targetManualPitch=rotationVelocity=pitchVelocity=focusHoldUntil=0;beginGlobeLayoutTransition(targetGlobeLayout(getFocusRegion(),baseRotation));lastGpuFrameState='';};
    const handleMapWheel=e=>{e.preventDefault();const b=canvas.getBoundingClientRect();applyManualZoom(manualZoomScale*Math.exp(-e.deltaY*.0012),e.clientX-b.left,e.clientY-b.top);};
    const pinchStart=e=>{if(e.touches.length!==2)return;const a=e.touches[0],b=e.touches[1];pinchStartDistance=Math.hypot(b.clientX-a.clientX,b.clientY-a.clientY);pinchStartZoomScale=manualZoomScale;activePointerId=null;dragSurface=null;touchGestureMode='pinch';rotationVelocity=pitchVelocity=0;lockCameraToCurrentFocus();e.preventDefault();};
    const pinchMove=e=>{if(pinchStartDistance<=0||e.touches.length!==2)return;const a=e.touches[0],b=e.touches[1],d=Math.hypot(b.clientX-a.clientX,b.clientY-a.clientY),r=canvas.getBoundingClientRect();applyManualZoom(pinchStartZoomScale*d/Math.max(pinchStartDistance,1),(a.clientX+b.clientX)*.5-r.left,(a.clientY+b.clientY)*.5-r.top);e.preventDefault();};
    const pinchEnd=e=>{if(e.touches.length<2){pinchStartDistance=0;if(touchGestureMode==='pinch')touchGestureMode=null;}};
    cameraControls.addEventListener('pointerdown',e=>e.stopPropagation());
    cameraControls.addEventListener('click',e=>{e.preventDefault();e.stopPropagation();const b=e.target.closest('[data-map-action]');if(!b)return;const a=b.dataset.mapAction;if(a==='zoom-in')applyManualZoom(manualZoomScale*1.22);else if(a==='zoom-out')applyManualZoom(manualZoomScale/1.22);else if(a==='fit-fastest')fitFastest();});
    root.addEventListener('wheel',handleMapWheel,{passive:false});
    root.addEventListener('touchstart',pinchStart,{passive:false,capture:true});root.addEventListener('touchmove',pinchMove,{passive:false,capture:true});root.addEventListener('touchend',pinchEnd,{passive:true,capture:true});root.addEventListener('touchcancel',pinchEnd,{passive:true,capture:true});

"""

CTRL_DOM="""    const label =
        root.querySelector(
            ".live-network-map__label-text"
        );
    const cameraControls=document.createElement('div');
    cameraControls.className='live-network-map__camera-controls';
    cameraControls.innerHTML='<button type="button" data-map-action="zoom-in" aria-label="Zoom in">+</button><button type="button" data-map-action="zoom-out" aria-label="Zoom out">−</button><button type="button" data-map-action="fit-fastest" aria-label="Fit your location and fastest WORK">◎</button>';
    root.appendChild(cameraControls);
"""

STYLE="""<style id='route-v7-controls'>.live-network-map__camera-controls{position:absolute;z-index:8;right:1rem;bottom:1rem;display:flex;flex-direction:column;gap:.55rem;pointer-events:auto}.live-network-map__camera-controls button{width:3.6rem;height:3.6rem;display:grid;place-items:center;border:1px solid rgba(255,255,255,.22);border-radius:.9rem;background:rgba(15,15,16,.82);color:#fff;font:600 1.9rem/1 'Poppins Local',Arial,sans-serif;cursor:pointer}.live-network-map__camera-controls button:hover{border-color:rgba(159,124,255,.72);background:rgba(45,35,82,.9)}@media(max-width:767.9px){.live-network-map__camera-controls{right:.35rem;bottom:.35rem}.live-network-map__camera-controls button{width:3.9rem;height:3.9rem;font-size:2rem}}</style>"""

def patch_js(s):
 s=region(s,'    const buildRouteVectors =','    const drawRoute =',ROUTE,'route')
 a=s.index('    const drawRoute ='); b=s.index('    /*\n     * Shared screen-space label allocator.',a); r=s[a:b]
 r=one(r,'            const point =\n                screenPoint(\n                    rotated\n                );\n',"""            let point =
                screenPoint(
                    rotated
                );
            if(index===routeVectors.length-1&&previous){const dx=point.x-previous.x,dy=point.y-previous.y,len=Math.hypot(dx,dy);if(len>.5){const inset=Math.min(nearest?9.2:6.8,len*.45);point={...point,x:point.x-dx/len*inset,y:point.y-dy/len*inset};}}
""",'end')
 s=s[:a]+r+s[b:]
 s=one(s,'"Approx. location"','"Your location (approx.)"','label')
 s=one(s,'    let hasManualInteraction = false;\n',STATE,'state')
 ts=s.index('    const targetGlobeLayout ='); te=s.index('    const beginGlobeLayoutTransition =',ts); t=s[ts:te]
 t=one(t,'            if (canvasIsFullscreen()) {\n                return base;\n            }\n',"""            const focusLocation=selectedServerLocation||lockedFocusLocation||scene.nearest?.location;
            if(canvasIsFullscreen()){return {...base,radius:base.radius*manualZoomScale};}
""",'focus')
 t=t.replace('!scene.nearest?.location','!focusLocation',1).replace('scene.nearest.location','focusLocation')
 t=one(t,'                        fittedRadius,\n                        maximumRadius\n','                        fittedRadius * manualZoomScale,\n                        maximumRadius * Math.max(1,manualZoomScale)\n','zoomtarget')
 s=s[:ts]+t+s[te:]
 s=one(s,'            if (globeLayoutTransition) {\n                return;\n            }\n','            if (globeLayoutTransition || cameraLockedByUser) {\n                return;\n            }\n','fitlock')
 s=one(s,'            hasManualInteraction =\n                true;\n','            hasManualInteraction =\n                true;\n            cameraLockedByUser=true;if(!lockedFocusLocation)lockedFocusLocation=selectedServerLocation||scene.nearest?.location||null;\n','draglock')
 s=one(s,'    const label =\n        root.querySelector(\n            ".live-network-map__label-text"\n        );\n',CTRL_DOM,'ctrl')
 p=s.index('    const resizeGlobe =\n'); s=s[:p]+CONTROL_JS+s[p:]
 s=one(s,'        openFullscreen = () => {\n',"""        openFullscreen = event => {
            if(event){const b=canvas.getBoundingClientRect(),x=event.clientX-b.left,y=event.clientY-b.top;let best=null,dist=Infinity;for(const t of nodeHitTargets){const d=Math.hypot(t.x-x,t.y-y);if(d<=t.hitRadius&&d<dist){best=t;dist=d;}}if(best){event.preventDefault();event.stopPropagation();focusOnServerLocation(best.location);return;}}
""",'dbl')
 ds=s.index('    const drawNode ='); de=s.index('    const drawScene =',ds); d=s[ds:de]
 d=one(d,'        const depthAlpha =\n','        nodeHitTargets.push({location,x:point.x,y:point.y,hitRadius:Math.max(36,options.radius+24)});\n\n        const depthAlpha =\n','hit')
 s=s[:ds]+d+s[de:]
 ss=s.index('    const drawScene ='); se=s.index('    const drawFrame =',ss); q=s[ss:se]
 q=one(q,'        labelRects.length = 0;\n','        labelRects.length = 0;\n        nodeHitTargets.length = 0;\n','clear')
 return s[:ss]+q+s[se:]

def patch_webgl(s,conf):
 s=one(s,'"rgba(111, 94, 183, .035)"','"rgba(111, 94, 183, 0)"','baked')
 old='''                drawCoastline(\n                    1.55,\n                    223 / 255,\n                    221 / 255,\n                    236 / 255,\n                    .46\n                );\n'''
 _,hw,ha,cw,ca=conf
 new=f'''                drawCoastline(\n                    {hw:.2f},\n                    156 / 255,\n                    123 / 255,\n                    255 / 255,\n                    {ha:.3f}\n                );\n                drawCoastline(\n                    {cw:.2f},\n                    223 / 255,\n                    221 / 255,\n                    236 / 255,\n                    {ca:.3f}\n                );\n'''
 return one(s,old,new,'coast')

for name,conf in V.items():
 j=patch_js(J); w=patch_webgl(W,conf)
 Path(f'network-map-{name}.js').write_text(j); Path(f'network-map-webgl-{name}.js').write_text(w)
 p=H.replace('/network-map-webgl-candidate-a.js?v=route-v6',f'/network-map-webgl-{name}.js?v=route-v7',1).replace('/network-map-candidate-a.js?v=route-v6',f'/network-map-{name}.js?v=route-v7',1).replace('</head>',STYLE+'</head>',1)
 title=html.escape(conf[0]); p=p.replace('</body>',f'<div style="position:fixed;right:12px;bottom:12px;z-index:2147483646;padding:8px 11px;border:1px solid #ffffff26;border-radius:8px;background:#080a16e8;color:#fff;font:12px system-ui;pointer-events:none"><b>{title}</b></div></body>',1)
 Path(f'{name}.html').write_text(p)

cards=''.join(f'<a href="/{n}.html"><b>{html.escape(c[0])}</b></a>' for n,c in V.items())
Path('route-v7-variants.html').write_text(f'<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1"><style>body{{background:#090b16;color:white;font:16px system-ui;padding:30px}}a{{display:block;color:white;padding:16px;margin:10px;border:1px solid #ffffff33}}</style><h1>Æthernet Route v7</h1>{cards}')
print('generated')

var e=Object.create,t=Object.defineProperty,n=Object.getOwnPropertyDescriptor,r=Object.getOwnPropertyNames,i=Object.getPrototypeOf,a=Object.prototype.hasOwnProperty,o=(e,t)=>()=>(t||e((t={exports:{}}).exports,t),t.exports),s=(e,i,o,s)=>{if(i&&typeof i==`object`||typeof i==`function`)for(var c=r(i),l=0,u=c.length,d;l<u;l++)d=c[l],!a.call(e,d)&&d!==o&&t(e,d,{get:(e=>i[e]).bind(null,d),enumerable:!(s=n(i,d))||s.enumerable});return e},c=(n,r,a)=>(a=n==null?{}:e(i(n)),s(r||!n||!n.__esModule?t(a,`default`,{value:n,enumerable:!0}):a,n));(function(){let e=document.createElement(`link`).relList;if(e&&e.supports&&e.supports(`modulepreload`))return;for(let e of document.querySelectorAll(`link[rel="modulepreload"]`))n(e);new MutationObserver(e=>{for(let t of e)if(t.type===`childList`)for(let e of t.addedNodes)e.tagName===`LINK`&&e.rel===`modulepreload`&&n(e)}).observe(document,{childList:!0,subtree:!0});function t(e){let t={};return e.integrity&&(t.integrity=e.integrity),e.referrerPolicy&&(t.referrerPolicy=e.referrerPolicy),e.crossOrigin===`use-credentials`?t.credentials=`include`:e.crossOrigin===`anonymous`?t.credentials=`omit`:t.credentials=`same-origin`,t}function n(e){if(e.ep)return;e.ep=!0;let n=t(e);fetch(e.href,n)}})();var l={context:void 0,registry:void 0,effects:void 0,done:!1,getContextId(){return u(this.context.count)},getNextContextId(){return u(this.context.count++)}};function u(e){let t=String(e),n=t.length-1;return l.context.id+(n?String.fromCharCode(96+n):``)+t}function d(e){l.context=e}function f(){return{...l.context,id:l.getNextContextId(),count:0}}var p=(e,t)=>e===t,m=Symbol(`solid-proxy`),h=typeof Proxy==`function`,g=Symbol(`solid-track`),_={equals:p},v=null,y=W,b=1,x=2,S={owned:null,cleanups:null,context:null,owner:null},C=null,w=null,T=null,E=null,D=null,O=null,k=null,A=0;function j(e,t){let n=D,r=C,i=e.length===0,a=t===void 0?r:t,o=i?S:{owned:null,cleanups:null,context:a?a.context:null,owner:a},s=i?e:()=>e(()=>I(()=>me(o)));C=o,D=null;try{return H(s,!0)}finally{D=n,C=r}}function M(e,t){t=t?Object.assign({},_,t):_;let n={value:e,observers:null,observerSlots:null,comparator:t.equals||void 0};return[ce.bind(n),e=>(typeof e==`function`&&(e=w&&w.running&&w.sources.has(n)?e(n.tValue):e(n.value)),R(n,e))]}function N(e,t,n){let r=B(e,t,!1,b);T&&w&&w.running?O.push(r):le(r)}function P(e,t,n){y=de;let r=B(e,t,!1,b),i=se&&L(se);i&&(r.suspense=i),(!n||!n.render)&&(r.user=!0),k?k.push(r):le(r)}function F(e,t,n){n=n?Object.assign({},_,n):_;let r=B(e,t,!0,0);return r.observers=null,r.observerSlots=null,r.comparator=n.equals||void 0,T&&w&&w.running?(r.tState=b,O.push(r)):le(r),ce.bind(r)}function ee(e){return H(e,!1)}function I(e){if(!E&&D===null)return e();let t=D;D=null;try{return E?E.untrack(e):e()}finally{D=t}}function te(e){P(()=>I(e))}function ne(e){return C===null||(C.cleanups===null?C.cleanups=[e]:C.cleanups.push(e)),e}function re(){return D}function ie(e){if(w&&w.running)return e(),w.done;let t=D,n=C;return Promise.resolve().then(()=>{D=t,C=n;let r;return(T||se)&&(r=w||={sources:new Set,effects:[],promises:new Set,disposed:new Set,queue:new Set,running:!0},r.done||=new Promise(e=>r.resolve=e),r.running=!0),H(e,!1),D=C=null,r?r.done:void 0})}var[ae,oe]=M(!1);function L(e){let t;return C&&C.context&&(t=C.context[e.id])!==void 0?t:e.defaultValue}var se;function ce(){let e=w&&w.running;if(this.sources&&(e?this.tState:this.state))if((e?this.tState:this.state)===b)le(this);else{let e=O;O=null,H(()=>fe(this),!1),O=e}if(D){let e=this.observers?this.observers.length:0;D.sources?(D.sources.push(this),D.sourceSlots.push(e)):(D.sources=[this],D.sourceSlots=[e]),this.observers?(this.observers.push(D),this.observerSlots.push(D.sources.length-1)):(this.observers=[D],this.observerSlots=[D.sources.length-1])}return e&&w.sources.has(this)?this.tValue:this.value}function R(e,t,n){let r=w&&w.running&&w.sources.has(e)?e.tValue:e.value;if(!e.comparator||!e.comparator(r,t)){if(w){let r=w.running;(r||!n&&w.sources.has(e))&&(w.sources.add(e),e.tValue=t),r||(e.value=t)}else e.value=t;e.observers&&e.observers.length&&H(()=>{for(let t=0;t<e.observers.length;t+=1){let n=e.observers[t],r=w&&w.running;r&&w.disposed.has(n)||((r?!n.tState:!n.state)&&(n.pure?O.push(n):k.push(n),n.observers&&pe(n)),r?n.tState=b:n.state=b)}if(O.length>1e6)throw O=[],Error()},!1)}return t}function le(e){if(!e.fn)return;me(e);let t=A;z(e,w&&w.running&&w.sources.has(e)?e.tValue:e.value,t),w&&!w.running&&w.sources.has(e)&&queueMicrotask(()=>{H(()=>{w&&(w.running=!0),D=C=e,z(e,e.tValue,t),D=C=null},!1)})}function z(e,t,n){let r,i=C,a=D;D=C=e;try{r=e.fn(t)}catch(t){return e.pure&&(w&&w.running?(e.tState=b,e.tOwned&&e.tOwned.forEach(me),e.tOwned=void 0):(e.state=b,e.owned&&e.owned.forEach(me),e.owned=null)),e.updatedAt=n+1,ve(t)}finally{D=a,C=i}(!e.updatedAt||e.updatedAt<=n)&&(e.updatedAt!=null&&`observers`in e?R(e,r,!0):w&&w.running&&e.pure?(w.sources.has(e)||(e.value=r),w.sources.add(e),e.tValue=r):e.value=r,e.updatedAt=n)}function B(e,t,n,r=b,i){let a={fn:e,state:r,updatedAt:null,owned:null,sources:null,sourceSlots:null,cleanups:null,value:t,owner:C,context:C?C.context:null,pure:n};if(w&&w.running&&(a.state=0,a.tState=r),C===null||C!==S&&(w&&w.running&&C.pure?C.tOwned?C.tOwned.push(a):C.tOwned=[a]:C.owned?C.owned.push(a):C.owned=[a]),E&&a.fn){let e=a.fn,[t,n]=M(void 0,{equals:!1}),r=E.factory(e,n);ne(()=>r.dispose());let i,o=()=>ie(n).then(()=>{i&&=(i.dispose(),void 0)});a.fn=n=>(t(),w&&w.running?(i||=E.factory(e,o),i.track(n)):r.track(n))}return a}function V(e){let t=w&&w.running;if((t?e.tState:e.state)===0)return;if((t?e.tState:e.state)===x)return fe(e);if(e.suspense&&I(e.suspense.inFallback))return e.suspense.effects.push(e);let n=[e];for(;(e=e.owner)&&(!e.updatedAt||e.updatedAt<A);){if(t&&w.disposed.has(e))return;(t?e.tState:e.state)&&n.push(e)}for(let r=n.length-1;r>=0;r--){if(e=n[r],t){let t=e,i=n[r+1];for(;(t=t.owner)&&t!==i;)if(w.disposed.has(t))return}if((t?e.tState:e.state)===b)le(e);else if((t?e.tState:e.state)===x){let t=O;O=null,H(()=>fe(e,n[0]),!1),O=t}}}function H(e,t){if(O)return e();let n=!1;t||(O=[]),k?n=!0:k=[],A++;try{let t=e();return U(n),t}catch(e){n||(k=null),O=null,ve(e)}}function U(e){if(O&&=(T&&w&&w.running?ue(O):W(O),null),e)return;let t;if(w){if(!w.promises.size&&!w.queue.size){let e=w.sources,n=w.disposed;k.push.apply(k,w.effects),t=w.resolve;for(let e of k)`tState`in e&&(e.state=e.tState),delete e.tState;w=null,H(()=>{for(let e of n)me(e);for(let t of e){if(t.value=t.tValue,t.owned)for(let e=0,n=t.owned.length;e<n;e++)me(t.owned[e]);t.tOwned&&(t.owned=t.tOwned),delete t.tValue,delete t.tOwned,t.tState=0}oe(!1)},!1)}else if(w.running){w.running=!1,w.effects.push.apply(w.effects,k),k=null,oe(!0);return}}let n=k;k=null,n.length&&H(()=>y(n),!1),t&&t()}function W(e){for(let t=0;t<e.length;t++)V(e[t])}function ue(e){for(let t=0;t<e.length;t++){let n=e[t],r=w.queue;r.has(n)||(r.add(n),T(()=>{r.delete(n),H(()=>{w.running=!0,V(n)},!1),w&&(w.running=!1)}))}}function de(e){let t,n=0;for(t=0;t<e.length;t++){let r=e[t];r.user?e[n++]=r:V(r)}if(l.context){if(l.count){l.effects||=[],l.effects.push(...e.slice(0,n));return}d()}for(l.effects&&(l.done||!l.count)&&(e=[...l.effects,...e],n+=l.effects.length,delete l.effects),t=0;t<n;t++)V(e[t])}function fe(e,t){let n=w&&w.running;n?e.tState=0:e.state=0;for(let r=0;r<e.sources.length;r+=1){let i=e.sources[r];if(i.sources){let e=n?i.tState:i.state;e===b?i!==t&&(!i.updatedAt||i.updatedAt<A)&&V(i):e===x&&fe(i,t)}}}function pe(e){let t=w&&w.running;for(let n=0;n<e.observers.length;n+=1){let r=e.observers[n];(t?!r.tState:!r.state)&&(t?r.tState=x:r.state=x,r.pure?O.push(r):k.push(r),r.observers&&pe(r))}}function me(e){let t;if(e.sources)for(;e.sources.length;){let t=e.sources.pop(),n=e.sourceSlots.pop(),r=t.observers;if(r&&r.length){let e=r.pop(),i=t.observerSlots.pop();n<r.length&&(e.sourceSlots[i]=n,r[n]=e,t.observerSlots[n]=i)}}if(e.tOwned){for(t=e.tOwned.length-1;t>=0;t--)me(e.tOwned[t]);delete e.tOwned}if(w&&w.running&&e.pure)he(e,!0);else if(e.owned){for(t=e.owned.length-1;t>=0;t--)me(e.owned[t]);e.owned=null}if(e.cleanups){for(t=e.cleanups.length-1;t>=0;t--)e.cleanups[t]();e.cleanups=null}w&&w.running?e.tState=0:e.state=0}function he(e,t){if(t||(e.tState=0,w.disposed.add(e)),e.owned)for(let t=0;t<e.owned.length;t++)he(e.owned[t])}function ge(e){return e instanceof Error?e:Error(typeof e==`string`?e:`Unknown error`,{cause:e})}function _e(e,t,n){try{for(let n of t)n(e)}catch(e){ve(e,n&&n.owner||null)}}function ve(e,t=C){let n=v&&t&&t.context&&t.context[v],r=ge(e);if(!n)throw r;k?k.push({fn(){_e(r,n,t)},state:b}):_e(r,n,t)}var ye=Symbol(`fallback`);function be(e){for(let t=0;t<e.length;t++)e[t]()}function xe(e,t,n={}){let r=[],i=[],a=[],o=0,s=t.length>1?[]:null;return ne(()=>be(a)),()=>{let c=e()||[],l=c.length,u,d;return c[g],I(()=>{let e,t,p,m,h,g,_,v,y;if(l===0)o!==0&&(be(a),a=[],r=[],i=[],o=0,s&&=[]),n.fallback&&(r=[ye],i[0]=j(e=>(a[0]=e,n.fallback())),o=1);else if(o===0){for(i=Array(l),d=0;d<l;d++)r[d]=c[d],i[d]=j(f);o=l}else{for(p=Array(l),m=Array(l),s&&(h=Array(l)),g=0,_=Math.min(o,l);g<_&&r[g]===c[g];g++);for(_=o-1,v=l-1;_>=g&&v>=g&&r[_]===c[v];_--,v--)p[v]=i[_],m[v]=a[_],s&&(h[v]=s[_]);for(e=new Map,t=Array(v+1),d=v;d>=g;d--)y=c[d],u=e.get(y),t[d]=u===void 0?-1:u,e.set(y,d);for(u=g;u<=_;u++)y=r[u],d=e.get(y),d!==void 0&&d!==-1?(p[d]=i[u],m[d]=a[u],s&&(h[d]=s[u]),d=t[d],e.set(y,d)):a[u]();for(d=g;d<l;d++)d in p?(i[d]=p[d],a[d]=m[d],s&&(s[d]=h[d],s[d](d))):i[d]=j(f);i=i.slice(0,o=l),r=c.slice(0)}return i});function f(e){if(a[d]=e,s){let[e,n]=M(d);return s[d]=n,t(c[d],e)}return t(c[d])}}}var Se=!1;function G(e,t){if(Se&&l.context){let n=l.context;d(f());let r=I(()=>e(t||{}));return d(n),r}return I(()=>e(t||{}))}function Ce(){return!0}var we={get(e,t,n){return t===m?n:e.get(t)},has(e,t){return t===m?!0:e.has(t)},set:Ce,deleteProperty:Ce,getOwnPropertyDescriptor(e,t){return{configurable:!0,enumerable:!0,get(){return e.get(t)},set:Ce,deleteProperty:Ce}},ownKeys(e){return e.keys()}};function Te(e){return(e=typeof e==`function`?e():e)?e:{}}function Ee(){for(let e=0,t=this.length;e<t;++e){let t=this[e]();if(t!==void 0)return t}}function De(...e){let t=!1;for(let n=0;n<e.length;n++){let r=e[n];t||=!!r&&m in r,e[n]=typeof r==`function`?(t=!0,F(r)):r}if(h&&t)return new Proxy({get(t){for(let n=e.length-1;n>=0;n--){let r=Te(e[n])[t];if(r!==void 0)return r}},has(t){for(let n=e.length-1;n>=0;n--)if(t in Te(e[n]))return!0;return!1},keys(){let t=[];for(let n=0;n<e.length;n++)t.push(...Object.keys(Te(e[n])));return[...new Set(t)]}},we);let n={},r=Object.create(null);for(let t=e.length-1;t>=0;t--){let i=e[t];if(!i)continue;let a=Object.getOwnPropertyNames(i);for(let e=a.length-1;e>=0;e--){let t=a[e];if(t===`__proto__`||t===`constructor`)continue;let o=Object.getOwnPropertyDescriptor(i,t);if(!r[t])r[t]=o.get?{enumerable:!0,configurable:!0,get:Ee.bind(n[t]=[o.get.bind(i)])}:o.value===void 0?void 0:o;else{let e=n[t];e&&(o.get?e.push(o.get.bind(i)):o.value!==void 0&&e.push(()=>o.value))}}}let i={},a=Object.keys(r);for(let e=a.length-1;e>=0;e--){let t=a[e],n=r[t];n&&n.get?Object.defineProperty(i,t,n):i[t]=n?n.value:void 0}return i}function Oe(e,...t){let n=t.length;if(h&&m in e){let r=n>1?t.flat():t[0],i=t.map(t=>new Proxy({get(n){return t.includes(n)?e[n]:void 0},has(n){return t.includes(n)&&n in e},keys(){return t.filter(t=>t in e)}},we));return i.push(new Proxy({get(t){return r.includes(t)?void 0:e[t]},has(t){return r.includes(t)?!1:t in e},keys(){return Object.keys(e).filter(e=>!r.includes(e))}},we)),i}let r=[];for(let e=0;e<=n;e++)r[e]={};for(let i of Object.getOwnPropertyNames(e)){let a=n;for(let e=0;e<t.length;e++)if(t[e].includes(i)){a=e;break}let o=Object.getOwnPropertyDescriptor(e,i);!o.get&&!o.set&&o.enumerable&&o.writable&&o.configurable?r[a][i]=o.value:Object.defineProperty(r[a],i,o)}return r}var ke=e=>`Stale read from <${e}>.`;function Ae(e){let t=`fallback`in e&&{fallback:()=>e.fallback};return F(xe(()=>e.each,e.children,t||void 0))}function je(e){let t=e.keyed,n=F(()=>e.when,void 0,void 0),r=t?n:F(n,void 0,{equals:(e,t)=>!e==!t});return F(()=>{let i=r();if(i){let a=e.children;return typeof a==`function`&&a.length>0?I(()=>a(t?i:()=>{if(!I(r))throw ke(`Show`);return n()})):a}return e.fallback},void 0,void 0)}var Me=new Set([`className`,`value`,`readOnly`,`noValidate`,`formNoValidate`,`isMap`,`noModule`,`playsInline`,`adAuctionHeaders`,`allowFullscreen`,`browsingTopics`,`defaultChecked`,`defaultMuted`,`defaultSelected`,`disablePictureInPicture`,`disableRemotePlayback`,`preservesPitch`,`shadowRootClonable`,`shadowRootCustomElementRegistry`,`shadowRootDelegatesFocus`,`shadowRootSerializable`,`sharedStorageWritable`,...`allowfullscreen.async.alpha.autofocus.autoplay.checked.controls.default.disabled.formnovalidate.hidden.indeterminate.inert.ismap.loop.multiple.muted.nomodule.novalidate.open.playsinline.readonly.required.reversed.seamless.selected.adauctionheaders.browsingtopics.credentialless.defaultchecked.defaultmuted.defaultselected.defer.disablepictureinpicture.disableremoteplayback.preservespitch.shadowrootclonable.shadowrootcustomelementregistry.shadowrootdelegatesfocus.shadowrootserializable.sharedstoragewritable`.split(`.`)]),Ne=new Set([`innerHTML`,`textContent`,`innerText`,`children`]),Pe=Object.assign(Object.create(null),{className:`class`,htmlFor:`for`}),Fe=Object.assign(Object.create(null),{class:`className`,novalidate:{$:`noValidate`,FORM:1},formnovalidate:{$:`formNoValidate`,BUTTON:1,INPUT:1},ismap:{$:`isMap`,IMG:1},nomodule:{$:`noModule`,SCRIPT:1},playsinline:{$:`playsInline`,VIDEO:1},readonly:{$:`readOnly`,INPUT:1,TEXTAREA:1},adauctionheaders:{$:`adAuctionHeaders`,IFRAME:1},allowfullscreen:{$:`allowFullscreen`,IFRAME:1},browsingtopics:{$:`browsingTopics`,IMG:1},defaultchecked:{$:`defaultChecked`,INPUT:1},defaultmuted:{$:`defaultMuted`,AUDIO:1,VIDEO:1},defaultselected:{$:`defaultSelected`,OPTION:1},disablepictureinpicture:{$:`disablePictureInPicture`,VIDEO:1},disableremoteplayback:{$:`disableRemotePlayback`,AUDIO:1,VIDEO:1},preservespitch:{$:`preservesPitch`,AUDIO:1,VIDEO:1},shadowrootclonable:{$:`shadowRootClonable`,TEMPLATE:1},shadowrootdelegatesfocus:{$:`shadowRootDelegatesFocus`,TEMPLATE:1},shadowrootserializable:{$:`shadowRootSerializable`,TEMPLATE:1},sharedstoragewritable:{$:`sharedStorageWritable`,IFRAME:1,IMG:1}});function Ie(e,t){let n=Fe[e];return typeof n==`object`?n[t]?n.$:void 0:n}var Le=new Set([`beforeinput`,`click`,`dblclick`,`contextmenu`,`focusin`,`focusout`,`input`,`keydown`,`keyup`,`mousedown`,`mousemove`,`mouseout`,`mouseover`,`mouseup`,`pointerdown`,`pointermove`,`pointerout`,`pointerover`,`pointerup`,`touchend`,`touchmove`,`touchstart`]),Re=new Set(`altGlyph.altGlyphDef.altGlyphItem.animate.animateColor.animateMotion.animateTransform.circle.clipPath.color-profile.cursor.defs.desc.ellipse.feBlend.feColorMatrix.feComponentTransfer.feComposite.feConvolveMatrix.feDiffuseLighting.feDisplacementMap.feDistantLight.feDropShadow.feFlood.feFuncA.feFuncB.feFuncG.feFuncR.feGaussianBlur.feImage.feMerge.feMergeNode.feMorphology.feOffset.fePointLight.feSpecularLighting.feSpotLight.feTile.feTurbulence.filter.font.font-face.font-face-format.font-face-name.font-face-src.font-face-uri.foreignObject.g.glyph.glyphRef.hkern.image.line.linearGradient.marker.mask.metadata.missing-glyph.mpath.path.pattern.polygon.polyline.radialGradient.rect.set.stop.svg.switch.symbol.text.textPath.tref.tspan.use.view.vkern`.split(`.`)),ze={xlink:`http://www.w3.org/1999/xlink`,xml:`http://www.w3.org/XML/1998/namespace`},Be=e=>F(()=>e());function Ve(e,t,n){let r=n.length,i=t.length,a=r,o=0,s=0,c=t[i-1].nextSibling,l=null;for(;o<i||s<a;){if(t[o]===n[s]){o++,s++;continue}for(;t[i-1]===n[a-1];)i--,a--;if(i===o){let t=a<r?s?n[s-1].nextSibling:n[a-s]:c;for(;s<a;)e.insertBefore(n[s++],t)}else if(a===s)for(;o<i;)(!l||!l.has(t[o]))&&t[o].remove(),o++;else if(t[o]===n[a-1]&&n[s]===t[i-1]){let r=t[--i].nextSibling;e.insertBefore(n[s++],t[o++].nextSibling),e.insertBefore(n[--a],r),t[i]=n[a]}else{if(!l){l=new Map;let e=s;for(;e<a;)l.set(n[e],e++)}let r=l.get(t[o]);if(r!=null)if(s<r&&r<a){let c=o,u=1,d;for(;++c<i&&c<a&&!((d=l.get(t[c]))==null||d!==r+u);)u++;if(u>r-s){let i=t[o];for(;s<r;)e.insertBefore(n[s++],i)}else e.replaceChild(n[s++],t[o++])}else o++;else t[o++].remove()}}}var He=`_$DX_DELEGATE`;function Ue(e,t,n,r={}){let i;return j(r=>{i=r,t===document?e():J(t,e(),t.firstChild?null:void 0,n)},r.owner),()=>{i(),t.textContent=``}}function K(e,t,n,r){let i,a=()=>{let t=r?document.createElementNS(`http://www.w3.org/1998/Math/MathML`,`template`):document.createElement(`template`);return t.innerHTML=e,n?t.content.firstChild.firstChild:r?t.firstChild:t.content.firstChild},o=t?()=>I(()=>document.importNode(i||=a(),!0)):()=>(i||=a()).cloneNode(!0);return o.cloneNode=o,o}function We(e,t=window.document){let n=t[He]||(t[He]=new Set);for(let r=0,i=e.length;r<i;r++){let i=e[r];n.has(i)||(n.add(i),t.addEventListener(i,ot))}}function Ge(e,t,n){nt(e)||(n==null?e.removeAttribute(t):e.setAttribute(t,n))}function Ke(e,t,n,r){nt(e)||(r==null?e.removeAttributeNS(t,n):e.setAttributeNS(t,n,r))}function qe(e,t,n){nt(e)||(n?e.setAttribute(t,``):e.removeAttribute(t))}function Je(e,t){nt(e)||(t==null?e.removeAttribute(`class`):e.className=t)}function q(e,t,n,r){if(r)Array.isArray(n)?(e[`$$${t}`]=n[0],e[`$$${t}Data`]=n[1]):e[`$$${t}`]=n;else if(Array.isArray(n)){let r=n[0];e.addEventListener(t,n[0]=t=>r.call(e,n[1],t))}else e.addEventListener(t,n,typeof n!=`function`&&n)}function Ye(e,t,n={}){let r=Object.keys(t||{}),i=Object.keys(n),a,o;for(a=0,o=i.length;a<o;a++){let r=i[a];!r||r===`undefined`||t[r]||(it(e,r,!1),delete n[r])}for(a=0,o=r.length;a<o;a++){let i=r[a],o=!!t[i];!i||i===`undefined`||n[i]===o||!o||(it(e,i,!0),n[i]=o)}return n}function Xe(e,t,n){if(!t)return n?Ge(e,`style`):t;let r=e.style;if(typeof t==`string`)return r.cssText=t;typeof n==`string`&&(r.cssText=n=void 0),n||={},t||={};let i,a;for(a in n)t[a]??r.removeProperty(a),delete n[a];for(a in t)i=t[a],i!==n[a]&&(r.setProperty(a,i),n[a]=i);return n}function Ze(e,t,n){n==null?e.style.removeProperty(t):e.style.setProperty(t,n)}function Qe(e,t={},n,r){let i={};return r||N(()=>i.children=st(e,t.children,i.children)),N(()=>typeof t.ref==`function`&&$e(t.ref,e)),N(()=>et(e,t,n,!0,i,!0)),i}function $e(e,t,n){return I(()=>e(t,n))}function J(e,t,n,r){if(n!==void 0&&!r&&(r=[]),typeof t!=`function`)return st(e,t,r,n);N(r=>st(e,t(),r,n),r)}function et(e,t,n,r,i={},a=!1){t||={};for(let r in i)if(!(r in t)){if(r===`children`)continue;i[r]=at(e,r,null,i[r],n,a,t)}for(let o in t){if(o===`children`){r||st(e,t.children);continue}let s=t[o];i[o]=at(e,o,s,i[o],n,a,t)}}function tt(e){let t,n;return!nt()||!(t=l.registry.get(n=dt()))?e():(l.completed&&l.completed.add(t),l.registry.delete(n),t)}function nt(e){return!!l.context&&!l.done&&(!e||e.isConnected)}function rt(e){return e.toLowerCase().replace(/-([a-z])/g,(e,t)=>t.toUpperCase())}function it(e,t,n){let r=t.trim().split(/\s+/);for(let t=0,i=r.length;t<i;t++)e.classList.toggle(r[t],n)}function at(e,t,n,r,i,a,o){let s,c,l,u,d;if(t===`style`)return Xe(e,n,r);if(t===`classList`)return Ye(e,n,r);if(n===r)return r;if(t===`ref`)a||n(e);else if(t.slice(0,3)===`on:`){let i=t.slice(3);r&&e.removeEventListener(i,r,typeof r!=`function`&&r),n&&e.addEventListener(i,n,typeof n!=`function`&&n)}else if(t.slice(0,10)===`oncapture:`){let i=t.slice(10);r&&e.removeEventListener(i,r,!0),n&&e.addEventListener(i,n,!0)}else if(t.slice(0,2)===`on`){let i=t.slice(2).toLowerCase(),a=Le.has(i);if(!a&&r){let t=Array.isArray(r)?r[0]:r;e.removeEventListener(i,t)}(a||n)&&(q(e,i,n,a),a&&We([i]))}else if(t.slice(0,5)===`attr:`)Ge(e,t.slice(5),n);else if(t.slice(0,5)===`bool:`)qe(e,t.slice(5),n);else if((d=t.slice(0,5)===`prop:`)||(l=Ne.has(t))||!i&&((u=Ie(t,e.tagName))||(c=Me.has(t)))||(s=e.nodeName.includes(`-`)||`is`in o)){if(d)t=t.slice(5),c=!0;else if(nt(e))return n;t===`class`||t===`className`?Je(e,n):s&&!c&&!l?e[rt(t)]=n:e[u||t]=n}else{let r=i&&t.indexOf(`:`)>-1&&ze[t.split(`:`)[0]];r?Ke(e,r,t,n):Ge(e,Pe[t]||t,n)}return n}function ot(e){if(l.registry&&l.events&&l.events.find(([t,n])=>n===e))return;let t=e.target,n=`$$${e.type}`,r=e.target,i=e.currentTarget,a=t=>Object.defineProperty(e,`target`,{configurable:!0,value:t}),o=()=>{let r=t[n];if(r&&!t.disabled){let i=t[`${n}Data`];if(i===void 0?r.call(t,e):r.call(t,i,e),e.cancelBubble)return}return t.host&&typeof t.host!=`string`&&!t.host._$host&&t.contains(e.target)&&a(t.host),!0},s=()=>{for(;o()&&(t=t._$host||t.parentNode||t.host););};if(Object.defineProperty(e,`currentTarget`,{configurable:!0,get(){return t||document}}),l.registry&&!l.done&&(l.done=_$HY.done=!0),e.composedPath){let n=e.composedPath();a(n[0]);for(let e=0;e<n.length-2&&(t=n[e],o());e++){if(t._$host){t=t._$host,s();break}if(t.parentNode===i)break}}else s();a(r)}function st(e,t,n,r,i){let a=nt(e);if(a){!n&&(n=[...e.childNodes]);let t=[];for(let e=0;e<n.length;e++){let r=n[e];r.nodeType===8&&r.data.slice(0,2)===`!$`?r.remove():t.push(r)}n=t}for(;typeof n==`function`;)n=n();if(t===n)return n;let o=typeof t,s=r!==void 0;if(e=s&&n[0]&&n[0].parentNode||e,o===`string`||o===`number`){if(a||o===`number`&&(t=t.toString(),t===n))return n;if(s){let i=n[0];i&&i.nodeType===3?i.data!==t&&(i.data=t):i=document.createTextNode(t),n=ut(e,n,r,i)}else n=n!==``&&typeof n==`string`?e.firstChild.data=t:e.textContent=t}else if(t==null||o===`boolean`){if(a)return n;n=ut(e,n,r)}else if(o===`function`)return N(()=>{let i=t();for(;typeof i==`function`;)i=i();n=st(e,i,n,r)}),()=>n;else if(Array.isArray(t)){let o=[],c=n&&Array.isArray(n);if(ct(o,t,n,i))return N(()=>n=st(e,o,n,r,!0)),()=>n;if(a){if(!o.length)return n;if(r===void 0)return n=[...e.childNodes];let t=o[0];if(t.parentNode!==e)return n;let i=[t];for(;(t=t.nextSibling)!==r;)i.push(t);return n=i}if(o.length===0){if(n=ut(e,n,r),s)return n}else c?n.length===0?lt(e,o,r):Ve(e,n,o):(n&&ut(e),lt(e,o));n=o}else if(t.nodeType){if(a&&t.parentNode)return n=s?[t]:t;if(Array.isArray(n)){if(s)return n=ut(e,n,r,t);ut(e,n,null,t)}else n==null||n===``||!e.firstChild?e.appendChild(t):e.replaceChild(t,e.firstChild);n=t}return n}function ct(e,t,n,r){let i=!1;for(let a=0,o=t.length;a<o;a++){let o=t[a],s=n&&n[e.length],c;if(!(o==null||o===!0||o===!1))if((c=typeof o)==`object`&&o.nodeType)e.push(o);else if(Array.isArray(o))i=ct(e,o,s)||i;else if(c===`function`)if(r){for(;typeof o==`function`;)o=o();i=ct(e,Array.isArray(o)?o:[o],Array.isArray(s)?s:[s])||i}else e.push(o),i=!0;else{let t=String(o);s&&s.nodeType===3&&s.data===t?e.push(s):e.push(document.createTextNode(t))}}return i}function lt(e,t,n=null){for(let r=0,i=t.length;r<i;r++)e.insertBefore(t[r],n)}function ut(e,t,n,r){if(n===void 0)return e.textContent=``;let i=r||document.createTextNode(``);if(t.length){let r=!1;for(let a=t.length-1;a>=0;a--){let o=t[a];if(i!==o){let t=o.parentNode===e;!r&&!a?t?e.replaceChild(i,o):e.insertBefore(i,n):t&&o.remove()}else r=!0}}else e.insertBefore(i,n);return[i]}function dt(){return l.getNextContextId()}var ft=`http://www.w3.org/2000/svg`;function pt(e,t=!1,n=void 0){return t?document.createElementNS(ft,e):document.createElement(e,{is:n})}function mt(e,t){let n=F(e);return F(()=>{let e=n();switch(typeof e){case`function`:return I(()=>e(t));case`string`:let n=Re.has(e),r=l.context?tt():pt(e,n,I(()=>t.is));return Qe(r,t,n),r}})}function ht(e){let[,t]=Oe(e,[`component`]);return mt(()=>e.component,t)}var gt=Symbol(`store-raw`),_t=Symbol(`store-node`),vt=Symbol(`store-has`),yt=Symbol(`store-self`);function bt(e){let t=e[m];if(!t&&(Object.defineProperty(e,m,{value:t=new Proxy(e,Ot)}),!Array.isArray(e))){let n=Object.keys(e),r=Object.getOwnPropertyDescriptors(e);for(let i=0,a=n.length;i<a;i++){let a=n[i];r[a].get&&Object.defineProperty(e,a,{enumerable:r[a].enumerable,get:r[a].get.bind(t)})}}return t}function xt(e){let t;return typeof e==`object`&&!!e&&(e[m]||!(t=Object.getPrototypeOf(e))||t===Object.prototype||Array.isArray(e))}function St(e,t=new Set){let n,r,i,a;if(n=e!=null&&e[gt])return n;if(!xt(e)||t.has(e))return e;if(Array.isArray(e)){Object.isFrozen(e)?e=e.slice(0):t.add(e);for(let n=0,a=e.length;n<a;n++)i=e[n],(r=St(i,t))!==i&&(e[n]=r)}else{Object.isFrozen(e)?e=Object.assign({},e):t.add(e);let n=Object.keys(e),o=Object.getOwnPropertyDescriptors(e);for(let s=0,c=n.length;s<c;s++)a=n[s],!o[a].get&&(i=e[a],(r=St(i,t))!==i&&(e[a]=r))}return e}function Ct(e,t){let n=e[t];return n||Object.defineProperty(e,t,{value:n=Object.create(null)}),n}function wt(e,t,n){if(e[t])return e[t];let[r,i]=M(n,{equals:!1,internal:!0});return r.$=i,e[t]=r}function Tt(e,t){let n=Reflect.getOwnPropertyDescriptor(e,t);return!n||n.get||!n.configurable||t===m||t===_t?n:(delete n.value,delete n.writable,n.get=()=>e[m][t],n)}function Et(e){re()&&wt(Ct(e,_t),yt)()}function Dt(e){return Et(e),Reflect.ownKeys(e)}var Ot={get(e,t,n){if(t===gt)return e;if(t===m)return n;if(t===g)return Et(e),n;let r=Ct(e,_t),i=r[t],a=i?i():e[t];if(t===_t||t===vt||t===`__proto__`)return a;if(!i){let n=Object.getOwnPropertyDescriptor(e,t);re()&&(typeof a!=`function`||e.hasOwnProperty(t))&&!(n&&n.get)&&(a=wt(r,t,a)())}return xt(a)?bt(a):a},has(e,t){return t===gt||t===m||t===g||t===_t||t===vt||t===`__proto__`?!0:(re()&&wt(Ct(e,vt),t)(),t in e)},set(){return!0},deleteProperty(){return!0},ownKeys:Dt,getOwnPropertyDescriptor:Tt};function kt(e,t,n,r=!1){if(!r&&e[t]===n)return;let i=e[t],a=e.length;n===void 0?(delete e[t],e[vt]&&e[vt][t]&&i!==void 0&&e[vt][t].$()):(e[t]=n,e[vt]&&e[vt][t]&&i===void 0&&e[vt][t].$());let o=Ct(e,_t),s;if((s=wt(o,t,i))&&s.$(()=>n),Array.isArray(e)&&e.length!==a){for(let t=e.length;t<a;t++)(s=o[t])&&s.$();(s=wt(o,`length`,a))&&s.$(e.length)}(s=o[yt])&&s.$()}function At(e,t){let n=Object.keys(t);for(let r=0;r<n.length;r+=1){let i=n[r];kt(e,i,t[i])}}function jt(e,t){if(typeof t==`function`&&(t=t(e)),t=St(t),Array.isArray(t)){if(e===t)return;let n=0,r=t.length;for(;n<r;n++){let r=t[n];e[n]!==r&&kt(e,n,r)}kt(e,`length`,r)}else At(e,t)}function Mt(e,t,n=[]){let r,i=e;if(t.length>1){r=t.shift();let a=typeof r,o=Array.isArray(e);if(Array.isArray(r)){for(let i=0;i<r.length;i++)Mt(e,[r[i]].concat(t),n);return}else if(o&&a===`function`){for(let i=0;i<e.length;i++)r(e[i],i)&&Mt(e,[i].concat(t),n);return}else if(o&&a===`object`){let{from:i=0,to:a=e.length-1,by:o=1}=r;for(let r=i;r<=a;r+=o)Mt(e,[r].concat(t),n);return}else if(t.length>1){Mt(e[r],t,[r].concat(n));return}i=e[r],n=[r].concat(n)}let a=t[0];typeof a==`function`&&(a=a(i,n),a===i)||r===void 0&&a==null||(a=St(a),r===void 0||xt(i)&&xt(a)&&!Array.isArray(a)?At(i,a):kt(e,r,a))}function Nt(...[e,t]){let n=St(e||{}),r=Array.isArray(n),i=bt(n);function a(...e){ee(()=>{r&&e.length===1?jt(n,e[0]):Mt(n,e)})}return[i,a]}var Pt=o(((e,t)=>{(function(n,r){typeof e==`object`&&t!==void 0?t.exports=r():typeof define==`function`&&define.amd?define(r):(n=typeof globalThis<`u`?globalThis:n||self).interact=r()})(e,(function(){function e(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);t&&(r=r.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,r)}return n}function n(t){for(var n=1;n<arguments.length;n++){var r=arguments[n]==null?{}:arguments[n];n%2?e(Object(r),!0).forEach((function(e){s(t,e,r[e])})):Object.getOwnPropertyDescriptors?Object.defineProperties(t,Object.getOwnPropertyDescriptors(r)):e(Object(r)).forEach((function(e){Object.defineProperty(t,e,Object.getOwnPropertyDescriptor(r,e))}))}return t}function r(e){return r=typeof Symbol==`function`&&typeof Symbol.iterator==`symbol`?function(e){return typeof e}:function(e){return e&&typeof Symbol==`function`&&e.constructor===Symbol&&e!==Symbol.prototype?`symbol`:typeof e},r(e)}function i(e,t){if(!(e instanceof t))throw TypeError(`Cannot call a class as a function`)}function a(e,t){for(var n=0;n<t.length;n++){var r=t[n];r.enumerable=r.enumerable||!1,r.configurable=!0,`value`in r&&(r.writable=!0),Object.defineProperty(e,m(r.key),r)}}function o(e,t,n){return t&&a(e.prototype,t),n&&a(e,n),Object.defineProperty(e,`prototype`,{writable:!1}),e}function s(e,t,n){return(t=m(t))in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function c(e,t){if(typeof t!=`function`&&t!==null)throw TypeError(`Super expression must either be null or a function`);e.prototype=Object.create(t&&t.prototype,{constructor:{value:e,writable:!0,configurable:!0}}),Object.defineProperty(e,`prototype`,{writable:!1}),t&&u(e,t)}function l(e){return l=Object.setPrototypeOf?Object.getPrototypeOf.bind():function(e){return e.__proto__||Object.getPrototypeOf(e)},l(e)}function u(e,t){return u=Object.setPrototypeOf?Object.setPrototypeOf.bind():function(e,t){return e.__proto__=t,e},u(e,t)}function d(e){if(e===void 0)throw ReferenceError(`this hasn't been initialised - super() hasn't been called`);return e}function f(e){var t=function(){if(typeof Reflect>`u`||!Reflect.construct||Reflect.construct.sham)return!1;if(typeof Proxy==`function`)return!0;try{return Boolean.prototype.valueOf.call(Reflect.construct(Boolean,[],(function(){}))),!0}catch{return!1}}();return function(){var n,r=l(e);if(t){var i=l(this).constructor;n=Reflect.construct(r,arguments,i)}else n=r.apply(this,arguments);return function(e,t){if(t&&(typeof t==`object`||typeof t==`function`))return t;if(t!==void 0)throw TypeError(`Derived constructors may only return object or undefined`);return d(e)}(this,n)}}function p(){return p=typeof Reflect<`u`&&Reflect.get?Reflect.get.bind():function(e,t,n){var r=function(e,t){for(;!Object.prototype.hasOwnProperty.call(e,t)&&(e=l(e))!==null;);return e}(e,t);if(r){var i=Object.getOwnPropertyDescriptor(r,t);return i.get?i.get.call(arguments.length<3?e:n):i.value}},p.apply(this,arguments)}function m(e){var t=function(e,t){if(typeof e!=`object`||!e)return e;var n=e[Symbol.toPrimitive];if(n!==void 0){var r=n.call(e,t||`default`);if(typeof r!=`object`)return r;throw TypeError(`@@toPrimitive must return a primitive value.`)}return(t===`string`?String:Number)(e)}(e,`string`);return typeof t==`symbol`?t:t+``}var h=function(e){return!(!e||!e.Window)&&e instanceof e.Window},g=void 0,_=void 0;function v(e){g=e;var t=e.document.createTextNode(``);t.ownerDocument!==e.document&&typeof e.wrap==`function`&&e.wrap(t)===t&&(e=e.wrap(e)),_=e}function y(e){return h(e)?e:(e.ownerDocument||e).defaultView||_.window}typeof window<`u`&&window&&v(window);var b=function(e){return!!e&&r(e)===`object`},x=function(e){return typeof e==`function`},S={window:function(e){return e===_||h(e)},docFrag:function(e){return b(e)&&e.nodeType===11},object:b,func:x,number:function(e){return typeof e==`number`},bool:function(e){return typeof e==`boolean`},string:function(e){return typeof e==`string`},element:function(e){if(!e||r(e)!==`object`)return!1;var t=y(e)||_;return/object|function/.test(typeof Element>`u`?`undefined`:r(Element))?e instanceof Element||e instanceof t.Element:e.nodeType===1&&typeof e.nodeName==`string`},plainObject:function(e){return b(e)&&!!e.constructor&&/function Object\b/.test(e.constructor.toString())},array:function(e){return b(e)&&e.length!==void 0&&x(e.splice)}};function C(e){var t=e.interaction;if(t.prepared.name===`drag`){var n=t.prepared.axis;n===`x`?(t.coords.cur.page.y=t.coords.start.page.y,t.coords.cur.client.y=t.coords.start.client.y,t.coords.velocity.client.y=0,t.coords.velocity.page.y=0):n===`y`&&(t.coords.cur.page.x=t.coords.start.page.x,t.coords.cur.client.x=t.coords.start.client.x,t.coords.velocity.client.x=0,t.coords.velocity.page.x=0)}}function w(e){var t=e.iEvent,n=e.interaction;if(n.prepared.name===`drag`){var r=n.prepared.axis;if(r===`x`||r===`y`){var i=r===`x`?`y`:`x`;t.page[i]=n.coords.start.page[i],t.client[i]=n.coords.start.client[i],t.delta[i]=0}}}var T={id:`actions/drag`,install:function(e){var t=e.actions,n=e.Interactable,r=e.defaults;n.prototype.draggable=T.draggable,t.map.drag=T,t.methodDict.drag=`draggable`,r.actions.drag=T.defaults},listeners:{"interactions:before-action-move":C,"interactions:action-resume":C,"interactions:action-move":w,"auto-start:check":function(e){var t=e.interaction,n=e.interactable,r=e.buttons,i=n.options.drag;if(i&&i.enabled&&(!t.pointerIsDown||!/mouse|pointer/.test(t.pointerType)||(r&n.options.drag.mouseButtons)!=0))return e.action={name:`drag`,axis:i.lockAxis===`start`?i.startAxis:i.lockAxis},!1}},draggable:function(e){return S.object(e)?(this.options.drag.enabled=!1!==e.enabled,this.setPerAction(`drag`,e),this.setOnEvents(`drag`,e),/^(xy|x|y|start)$/.test(e.lockAxis)&&(this.options.drag.lockAxis=e.lockAxis),/^(xy|x|y)$/.test(e.startAxis)&&(this.options.drag.startAxis=e.startAxis),this):S.bool(e)?(this.options.drag.enabled=e,this):this.options.drag},beforeMove:C,move:w,defaults:{startAxis:`xy`,lockAxis:`xy`},getCursor:function(){return`move`},filterEventType:function(e){return e.search(`drag`)===0}},E=T,D={init:function(e){var t=e;D.document=t.document,D.DocumentFragment=t.DocumentFragment||O,D.SVGElement=t.SVGElement||O,D.SVGSVGElement=t.SVGSVGElement||O,D.SVGElementInstance=t.SVGElementInstance||O,D.Element=t.Element||O,D.HTMLElement=t.HTMLElement||D.Element,D.Event=t.Event,D.Touch=t.Touch||O,D.PointerEvent=t.PointerEvent||t.MSPointerEvent},document:null,DocumentFragment:null,SVGElement:null,SVGSVGElement:null,SVGElementInstance:null,Element:null,HTMLElement:null,Event:null,Touch:null,PointerEvent:null};function O(){}var k=D,A={init:function(e){var t=k.Element,n=e.navigator||{};A.supportsTouch=`ontouchstart`in e||S.func(e.DocumentTouch)&&k.document instanceof e.DocumentTouch,A.supportsPointerEvent=!1!==n.pointerEnabled&&!!k.PointerEvent,A.isIOS=/iP(hone|od|ad)/.test(n.platform),A.isIOS7=/iP(hone|od|ad)/.test(n.platform)&&/OS 7[^\d]/.test(n.appVersion),A.isIe9=/MSIE 9/.test(n.userAgent),A.isOperaMobile=n.appName===`Opera`&&A.supportsTouch&&/Presto/.test(n.userAgent),A.prefixedMatchesSelector=`matches`in t.prototype?`matches`:`webkitMatchesSelector`in t.prototype?`webkitMatchesSelector`:`mozMatchesSelector`in t.prototype?`mozMatchesSelector`:`oMatchesSelector`in t.prototype?`oMatchesSelector`:`msMatchesSelector`,A.pEventTypes=A.supportsPointerEvent?k.PointerEvent===e.MSPointerEvent?{up:`MSPointerUp`,down:`MSPointerDown`,over:`mouseover`,out:`mouseout`,move:`MSPointerMove`,cancel:`MSPointerCancel`}:{up:`pointerup`,down:`pointerdown`,over:`pointerover`,out:`pointerout`,move:`pointermove`,cancel:`pointercancel`}:null,A.wheelEvent=k.document&&`onmousewheel`in k.document?`mousewheel`:`wheel`},supportsTouch:null,supportsPointerEvent:null,isIOS7:null,isIOS:null,isIe9:null,isOperaMobile:null,prefixedMatchesSelector:null,pEventTypes:null,wheelEvent:null},j=A;function M(e,t){if(e.contains)return e.contains(t);for(;t;){if(t===e)return!0;t=t.parentNode}return!1}function N(e,t){for(;S.element(e);){if(F(e,t))return e;e=P(e)}return null}function P(e){var t=e.parentNode;if(S.docFrag(t)){for(;(t=t.host)&&S.docFrag(t););return t}return t}function F(e,t){return _!==g&&(t=t.replace(/\/deep\//g,` `)),e[j.prefixedMatchesSelector](t)}var ee=function(e){return e.parentNode||e.host};function I(e,t){for(var n,r=[],i=e;(n=ee(i))&&i!==t&&n!==i.ownerDocument;)r.unshift(i),i=n;return r}function te(e,t,n){for(;S.element(e);){if(F(e,t))return!0;if((e=P(e))===n)return F(e,t)}return!1}function ne(e){return e.correspondingUseElement||e}function re(e){var t=e instanceof k.SVGElement?e.getBoundingClientRect():e.getClientRects()[0];return t&&{left:t.left,right:t.right,top:t.top,bottom:t.bottom,width:t.width||t.right-t.left,height:t.height||t.bottom-t.top}}function ie(e){var t,n=re(e);if(!j.isIOS7&&n){var r={x:(t=(t=y(e))||_).scrollX||t.document.documentElement.scrollLeft,y:t.scrollY||t.document.documentElement.scrollTop};n.left+=r.x,n.right+=r.x,n.top+=r.y,n.bottom+=r.y}return n}function ae(e){for(var t=[];e;)t.push(e),e=P(e);return t}function oe(e){return!!S.string(e)&&(k.document.querySelector(e),!0)}function L(e,t){for(var n in t)e[n]=t[n];return e}function se(e,t,n){return e===`parent`?P(n):e===`self`?t.getRect(n):N(n,e)}function ce(e,t,n,r){var i=e;return S.string(i)?i=se(i,t,n):S.func(i)&&(i=i.apply(void 0,r)),S.element(i)&&(i=ie(i)),i}function R(e){return e&&{x:`x`in e?e.x:e.left,y:`y`in e?e.y:e.top}}function le(e){return!e||`x`in e&&`y`in e||((e=L({},e)).x=e.left||0,e.y=e.top||0,e.width=e.width||(e.right||0)-e.x,e.height=e.height||(e.bottom||0)-e.y),e}function z(e,t,n){e.left&&(t.left+=n.x),e.right&&(t.right+=n.x),e.top&&(t.top+=n.y),e.bottom&&(t.bottom+=n.y),t.width=t.right-t.left,t.height=t.bottom-t.top}function B(e,t,n){var r=n&&e.options[n];return R(ce(r&&r.origin||e.options.origin,e,t,[e&&t]))||{x:0,y:0}}function V(e,t){var n=arguments.length>2&&arguments[2]!==void 0?arguments[2]:function(e){return!0},r=arguments.length>3?arguments[3]:void 0;if(r||={},S.string(e)&&e.search(` `)!==-1&&(e=H(e)),S.array(e))return e.forEach((function(e){return V(e,t,n,r)})),r;if(S.object(e)&&(t=e,e=``),S.func(t)&&n(e))r[e]=r[e]||[],r[e].push(t);else if(S.array(t))for(var i=0,a=t;i<a.length;i++){var o=a[i];V(e,o,n,r)}else if(S.object(t))for(var s in t)V(H(s).map((function(t){return`${e}${t}`})),t[s],n,r);return r}function H(e){return e.trim().split(/ +/)}var U=function(e,t){return Math.sqrt(e*e+t*t)},W=[`webkit`,`moz`];function ue(e,t){e.__set||={};var n=function(n){if(W.some((function(e){return n.indexOf(e)===0})))return 1;typeof e[n]!=`function`&&n!==`__set`&&Object.defineProperty(e,n,{get:function(){return n in e.__set?e.__set[n]:e.__set[n]=t[n]},set:function(t){e.__set[n]=t},configurable:!0})};for(var r in t)n(r);return e}function de(e,t){e.page=e.page||{},e.page.x=t.page.x,e.page.y=t.page.y,e.client=e.client||{},e.client.x=t.client.x,e.client.y=t.client.y,e.timeStamp=t.timeStamp}function fe(e){e.page.x=0,e.page.y=0,e.client.x=0,e.client.y=0}function pe(e){return e instanceof k.Event||e instanceof k.Touch}function me(e,t,n){return e||=`page`,(n||={}).x=t[e+`X`],n.y=t[e+`Y`],n}function he(e,t){return t||={x:0,y:0},j.isOperaMobile&&pe(e)?(me(`screen`,e,t),t.x+=window.scrollX,t.y+=window.scrollY):me(`page`,e,t),t}function ge(e){return S.number(e.pointerId)?e.pointerId:e.identifier}function _e(e,t,n){var r=t.length>1?ye(t):t[0];he(r,e.page),function(e,t){t||={},j.isOperaMobile&&pe(e)?me(`screen`,e,t):me(`client`,e,t)}(r,e.client),e.timeStamp=n}function ve(e){var t=[];return S.array(e)?(t[0]=e[0],t[1]=e[1]):e.type===`touchend`?e.touches.length===1?(t[0]=e.touches[0],t[1]=e.changedTouches[0]):e.touches.length===0&&(t[0]=e.changedTouches[0],t[1]=e.changedTouches[1]):(t[0]=e.touches[0],t[1]=e.touches[1]),t}function ye(e){for(var t={pageX:0,pageY:0,clientX:0,clientY:0,screenX:0,screenY:0},n=0;n<e.length;n++){var r=e[n];for(var i in t)t[i]+=r[i]}for(var a in t)t[a]/=e.length;return t}function be(e){if(!e.length)return null;var t=ve(e),n=Math.min(t[0].pageX,t[1].pageX),r=Math.min(t[0].pageY,t[1].pageY),i=Math.max(t[0].pageX,t[1].pageX),a=Math.max(t[0].pageY,t[1].pageY);return{x:n,y:r,left:n,top:r,right:i,bottom:a,width:i-n,height:a-r}}function xe(e,t){var n=t+`X`,r=t+`Y`,i=ve(e);return U(i[0][n]-i[1][n],i[0][r]-i[1][r])}function Se(e,t){var n=t+`X`,r=t+`Y`,i=ve(e),a=i[1][n]-i[0][n],o=i[1][r]-i[0][r];return 180*Math.atan2(o,a)/Math.PI}function G(e){return S.string(e.pointerType)?e.pointerType:S.number(e.pointerType)?[void 0,void 0,`touch`,`pen`,`mouse`][e.pointerType]:/touch/.test(e.type||``)||e instanceof k.Touch?`touch`:`mouse`}function Ce(e){var t=S.func(e.composedPath)?e.composedPath():e.path;return[ne(t?t[0]:e.target),ne(e.currentTarget)]}var we=function(){function e(t){i(this,e),this.immediatePropagationStopped=!1,this.propagationStopped=!1,this._interaction=t}return o(e,[{key:`preventDefault`,value:function(){}},{key:`stopPropagation`,value:function(){this.propagationStopped=!0}},{key:`stopImmediatePropagation`,value:function(){this.immediatePropagationStopped=this.propagationStopped=!0}}]),e}();Object.defineProperty(we.prototype,`interaction`,{get:function(){return this._interaction._proxy},set:function(){}});var Te=function(e,t){for(var n=0;n<t.length;n++){var r=t[n];e.push(r)}return e},Ee=function(e){return Te([],e)},De=function(e,t){for(var n=0;n<e.length;n++)if(t(e[n],n,e))return n;return-1},Oe=function(e,t){return e[De(e,t)]},ke=function(e){c(n,e);var t=f(n);function n(e,r,a){var o;i(this,n),(o=t.call(this,r._interaction)).dropzone=void 0,o.dragEvent=void 0,o.relatedTarget=void 0,o.draggable=void 0,o.propagationStopped=!1,o.immediatePropagationStopped=!1;var s=a===`dragleave`?e.prev:e.cur,c=s.element,l=s.dropzone;return o.type=a,o.target=c,o.currentTarget=c,o.dropzone=l,o.dragEvent=r,o.relatedTarget=r.target,o.draggable=r.interactable,o.timeStamp=r.timeStamp,o}return o(n,[{key:`reject`,value:function(){var e=this,t=this._interaction.dropState;if(this.type===`dropactivate`||this.dropzone&&t.cur.dropzone===this.dropzone&&t.cur.element===this.target)if(t.prev.dropzone=this.dropzone,t.prev.element=this.target,t.rejected=!0,t.events.enter=null,this.stopImmediatePropagation(),this.type===`dropactivate`){var r=t.activeDrops,i=De(r,(function(t){var n=t.dropzone,r=t.element;return n===e.dropzone&&r===e.target}));t.activeDrops.splice(i,1);var a=new n(t,this.dragEvent,`dropdeactivate`);a.dropzone=this.dropzone,a.target=this.target,this.dropzone.fire(a)}else this.dropzone.fire(new n(t,this.dragEvent,`dragleave`))}},{key:`preventDefault`,value:function(){}},{key:`stopPropagation`,value:function(){this.propagationStopped=!0}},{key:`stopImmediatePropagation`,value:function(){this.immediatePropagationStopped=this.propagationStopped=!0}}]),n}(we);function Ae(e,t){for(var n=0,r=e.slice();n<r.length;n++){var i=r[n],a=i.dropzone,o=i.element;t.dropzone=a,t.target=o,a.fire(t),t.propagationStopped=t.immediatePropagationStopped=!1}}function je(e,t){for(var n=function(e,t){for(var n=[],r=0,i=e.interactables.list;r<i.length;r++){var a=i[r];if(a.options.drop.enabled){var o=a.options.drop.accept;if(!(S.element(o)&&o!==t||S.string(o)&&!F(t,o)||S.func(o)&&!o({dropzone:a,draggableElement:t})))for(var s=0,c=a.getAllElements();s<c.length;s++){var l=c[s];l!==t&&n.push({dropzone:a,element:l,rect:a.getRect(l)})}}}return n}(e,t),r=0;r<n.length;r++){var i=n[r];i.rect=i.dropzone.getRect(i.element)}return n}function Me(e,t,n){for(var r=e.dropState,i=e.interactable,a=e.element,o=[],s=0,c=r.activeDrops;s<c.length;s++){var l=c[s],u=l.dropzone,d=l.element,f=l.rect,p=u.dropCheck(t,n,i,a,d,f);o.push(p?d:null)}var m=function(e){for(var t,n,r,i=[],a=0;a<e.length;a++){var o=e[a],s=e[t];if(o&&a!==t)if(s){var c=ee(o),l=ee(s);if(c!==o.ownerDocument)if(l!==o.ownerDocument)if(c!==l){i=i.length?i:I(s);var u=void 0;if(s instanceof k.HTMLElement&&o instanceof k.SVGElement&&!(o instanceof k.SVGSVGElement)){if(o===l)continue;u=o.ownerSVGElement}else u=o;for(var d=I(u,s.ownerDocument),f=0;d[f]&&d[f]===i[f];)f++;var p=[d[f-1],d[f],i[f]];if(p[0])for(var m=p[0].lastChild;m;){if(m===p[1]){t=a,i=d;break}if(m===p[2])break;m=m.previousSibling}}else r=s,(parseInt(y(n=o).getComputedStyle(n).zIndex,10)||0)>=(parseInt(y(r).getComputedStyle(r).zIndex,10)||0)&&(t=a);else t=a}else t=a}return t}(o);return r.activeDrops[m]||null}function Ne(e,t,n){var r=e.dropState,i={enter:null,leave:null,activate:null,deactivate:null,move:null,drop:null};return n.type===`dragstart`&&(i.activate=new ke(r,n,`dropactivate`),i.activate.target=null,i.activate.dropzone=null),n.type===`dragend`&&(i.deactivate=new ke(r,n,`dropdeactivate`),i.deactivate.target=null,i.deactivate.dropzone=null),r.rejected||(r.cur.element!==r.prev.element&&(r.prev.dropzone&&(i.leave=new ke(r,n,`dragleave`),n.dragLeave=i.leave.target=r.prev.element,n.prevDropzone=i.leave.dropzone=r.prev.dropzone),r.cur.dropzone&&(i.enter=new ke(r,n,`dragenter`),n.dragEnter=r.cur.element,n.dropzone=r.cur.dropzone)),n.type===`dragend`&&r.cur.dropzone&&(i.drop=new ke(r,n,`drop`),n.dropzone=r.cur.dropzone,n.relatedTarget=r.cur.element),n.type===`dragmove`&&r.cur.dropzone&&(i.move=new ke(r,n,`dropmove`),n.dropzone=r.cur.dropzone)),i}function Pe(e,t){var n=e.dropState,r=n.activeDrops,i=n.cur,a=n.prev;t.leave&&a.dropzone.fire(t.leave),t.enter&&i.dropzone.fire(t.enter),t.move&&i.dropzone.fire(t.move),t.drop&&i.dropzone.fire(t.drop),t.deactivate&&Ae(r,t.deactivate),n.prev.dropzone=i.dropzone,n.prev.element=i.element}function Fe(e,t){var n=e.interaction,r=e.iEvent,i=e.event;if(r.type===`dragmove`||r.type===`dragend`){var a=n.dropState;t.dynamicDrop&&(a.activeDrops=je(t,n.element));var o=r,s=Me(n,o,i);a.rejected=a.rejected&&!!s&&s.dropzone===a.cur.dropzone&&s.element===a.cur.element,a.cur.dropzone=s&&s.dropzone,a.cur.element=s&&s.element,a.events=Ne(n,0,o)}}var Ie={id:`actions/drop`,install:function(e){var t=e.actions,n=e.interactStatic,r=e.Interactable,i=e.defaults;e.usePlugin(E),r.prototype.dropzone=function(e){return function(e,t){if(S.object(t)){if(e.options.drop.enabled=!1!==t.enabled,t.listeners){var n=V(t.listeners),r=Object.keys(n).reduce((function(e,t){return e[/^(enter|leave)/.test(t)?`drag${t}`:/^(activate|deactivate|move)/.test(t)?`drop${t}`:t]=n[t],e}),{}),i=e.options.drop.listeners;i&&e.off(i),e.on(r),e.options.drop.listeners=r}return S.func(t.ondrop)&&e.on(`drop`,t.ondrop),S.func(t.ondropactivate)&&e.on(`dropactivate`,t.ondropactivate),S.func(t.ondropdeactivate)&&e.on(`dropdeactivate`,t.ondropdeactivate),S.func(t.ondragenter)&&e.on(`dragenter`,t.ondragenter),S.func(t.ondragleave)&&e.on(`dragleave`,t.ondragleave),S.func(t.ondropmove)&&e.on(`dropmove`,t.ondropmove),/^(pointer|center)$/.test(t.overlap)?e.options.drop.overlap=t.overlap:S.number(t.overlap)&&(e.options.drop.overlap=Math.max(Math.min(1,t.overlap),0)),`accept`in t&&(e.options.drop.accept=t.accept),`checker`in t&&(e.options.drop.checker=t.checker),e}return S.bool(t)?(e.options.drop.enabled=t,e):e.options.drop}(this,e)},r.prototype.dropCheck=function(e,t,n,r,i,a){return function(e,t,n,r,i,a,o){var s=!1;if(!(o||=e.getRect(a)))return!!e.options.drop.checker&&e.options.drop.checker(t,n,s,e,a,r,i);var c=e.options.drop.overlap;if(c===`pointer`){var l=B(r,i,`drag`),u=he(t);u.x+=l.x,u.y+=l.y;var d=u.x>o.left&&u.x<o.right,f=u.y>o.top&&u.y<o.bottom;s=d&&f}var p=r.getRect(i);if(p&&c===`center`){var m=p.left+p.width/2,h=p.top+p.height/2;s=m>=o.left&&m<=o.right&&h>=o.top&&h<=o.bottom}return p&&S.number(c)&&(s=Math.max(0,Math.min(o.right,p.right)-Math.max(o.left,p.left))*Math.max(0,Math.min(o.bottom,p.bottom)-Math.max(o.top,p.top))/(p.width*p.height)>=c),e.options.drop.checker&&(s=e.options.drop.checker(t,n,s,e,a,r,i)),s}(this,e,t,n,r,i,a)},n.dynamicDrop=function(t){return S.bool(t)?(e.dynamicDrop=t,n):e.dynamicDrop},L(t.phaselessTypes,{dragenter:!0,dragleave:!0,dropactivate:!0,dropdeactivate:!0,dropmove:!0,drop:!0}),t.methodDict.drop=`dropzone`,e.dynamicDrop=!1,i.actions.drop=Ie.defaults},listeners:{"interactions:before-action-start":function(e){var t=e.interaction;t.prepared.name===`drag`&&(t.dropState={cur:{dropzone:null,element:null},prev:{dropzone:null,element:null},rejected:null,events:null,activeDrops:[]})},"interactions:after-action-start":function(e,t){var n=e.interaction,r=(e.event,e.iEvent);if(n.prepared.name===`drag`){var i=n.dropState;i.activeDrops=[],i.events={},i.activeDrops=je(t,n.element),i.events=Ne(n,0,r),i.events.activate&&(Ae(i.activeDrops,i.events.activate),t.fire(`actions/drop:start`,{interaction:n,dragEvent:r}))}},"interactions:action-move":Fe,"interactions:after-action-move":function(e,t){var n=e.interaction,r=e.iEvent;if(n.prepared.name===`drag`){var i=n.dropState;Pe(n,i.events),t.fire(`actions/drop:move`,{interaction:n,dragEvent:r}),i.events={}}},"interactions:action-end":function(e,t){if(e.interaction.prepared.name===`drag`){var n=e.interaction,r=e.iEvent;Fe(e,t),Pe(n,n.dropState.events),t.fire(`actions/drop:end`,{interaction:n,dragEvent:r})}},"interactions:stop":function(e){var t=e.interaction;if(t.prepared.name===`drag`){var n=t.dropState;n&&(n.activeDrops=null,n.events=null,n.cur.dropzone=null,n.cur.element=null,n.prev.dropzone=null,n.prev.element=null,n.rejected=!1)}}},getActiveDrops:je,getDrop:Me,getDropEvents:Ne,fireDropEvents:Pe,filterEventType:function(e){return e.search(`drag`)===0||e.search(`drop`)===0},defaults:{enabled:!1,accept:null,overlap:`pointer`}},Le=Ie;function Re(e){var t=e.interaction,n=e.iEvent,r=e.phase;if(t.prepared.name===`gesture`){var i=t.pointers.map((function(e){return e.pointer})),a=r===`start`,o=r===`end`,s=t.interactable.options.deltaSource;if(n.touches=[i[0],i[1]],a)n.distance=xe(i,s),n.box=be(i),n.scale=1,n.ds=0,n.angle=Se(i,s),n.da=0,t.gesture.startDistance=n.distance,t.gesture.startAngle=n.angle;else if(o||t.pointers.length<2){var c=t.prevEvent;n.distance=c.distance,n.box=c.box,n.scale=c.scale,n.ds=0,n.angle=c.angle,n.da=0}else n.distance=xe(i,s),n.box=be(i),n.scale=n.distance/t.gesture.startDistance,n.angle=Se(i,s),n.ds=n.scale-t.gesture.scale,n.da=n.angle-t.gesture.angle;t.gesture.distance=n.distance,t.gesture.angle=n.angle,S.number(n.scale)&&n.scale!==1/0&&!isNaN(n.scale)&&(t.gesture.scale=n.scale)}}var ze={id:`actions/gesture`,before:[`actions/drag`,`actions/resize`],install:function(e){var t=e.actions,n=e.Interactable,r=e.defaults;n.prototype.gesturable=function(e){return S.object(e)?(this.options.gesture.enabled=!1!==e.enabled,this.setPerAction(`gesture`,e),this.setOnEvents(`gesture`,e),this):S.bool(e)?(this.options.gesture.enabled=e,this):this.options.gesture},t.map.gesture=ze,t.methodDict.gesture=`gesturable`,r.actions.gesture=ze.defaults},listeners:{"interactions:action-start":Re,"interactions:action-move":Re,"interactions:action-end":Re,"interactions:new":function(e){e.interaction.gesture={angle:0,distance:0,scale:1,startAngle:0,startDistance:0}},"auto-start:check":function(e){if(!(e.interaction.pointers.length<2)){var t=e.interactable.options.gesture;if(t&&t.enabled)return e.action={name:`gesture`},!1}}},defaults:{},getCursor:function(){return``},filterEventType:function(e){return e.search(`gesture`)===0}},Be=ze;function Ve(e,t,n,r,i,a,o){if(!t)return!1;if(!0===t){var s=S.number(a.width)?a.width:a.right-a.left,c=S.number(a.height)?a.height:a.bottom-a.top;if(o=Math.min(o,Math.abs((e===`left`||e===`right`?s:c)/2)),s<0&&(e===`left`?e=`right`:e===`right`&&(e=`left`)),c<0&&(e===`top`?e=`bottom`:e===`bottom`&&(e=`top`)),e===`left`){var l=s>=0?a.left:a.right;return n.x<l+o}if(e===`top`){var u=c>=0?a.top:a.bottom;return n.y<u+o}if(e===`right`)return n.x>(s>=0?a.right:a.left)-o;if(e===`bottom`)return n.y>(c>=0?a.bottom:a.top)-o}return!!S.element(r)&&(S.element(t)?t===r:te(r,t,i))}function He(e){var t=e.iEvent,n=e.interaction;if(n.prepared.name===`resize`&&n.resizeAxes){var r=t;n.interactable.options.resize.square?(n.resizeAxes===`y`?r.delta.x=r.delta.y:r.delta.y=r.delta.x,r.axes=`xy`):(r.axes=n.resizeAxes,n.resizeAxes===`x`?r.delta.y=0:n.resizeAxes===`y`&&(r.delta.x=0))}}var Ue,K,We={id:`actions/resize`,before:[`actions/drag`],install:function(e){var t=e.actions,n=e.browser,r=e.Interactable,i=e.defaults;We.cursors=function(e){return e.isIe9?{x:`e-resize`,y:`s-resize`,xy:`se-resize`,top:`n-resize`,left:`w-resize`,bottom:`s-resize`,right:`e-resize`,topleft:`se-resize`,bottomright:`se-resize`,topright:`ne-resize`,bottomleft:`ne-resize`}:{x:`ew-resize`,y:`ns-resize`,xy:`nwse-resize`,top:`ns-resize`,left:`ew-resize`,bottom:`ns-resize`,right:`ew-resize`,topleft:`nwse-resize`,bottomright:`nwse-resize`,topright:`nesw-resize`,bottomleft:`nesw-resize`}}(n),We.defaultMargin=n.supportsTouch||n.supportsPointerEvent?20:10,r.prototype.resizable=function(t){return function(e,t,n){return S.object(t)?(e.options.resize.enabled=!1!==t.enabled,e.setPerAction(`resize`,t),e.setOnEvents(`resize`,t),S.string(t.axis)&&/^x$|^y$|^xy$/.test(t.axis)?e.options.resize.axis=t.axis:t.axis===null&&(e.options.resize.axis=n.defaults.actions.resize.axis),S.bool(t.preserveAspectRatio)?e.options.resize.preserveAspectRatio=t.preserveAspectRatio:S.bool(t.square)&&(e.options.resize.square=t.square),e):S.bool(t)?(e.options.resize.enabled=t,e):e.options.resize}(this,t,e)},t.map.resize=We,t.methodDict.resize=`resizable`,i.actions.resize=We.defaults},listeners:{"interactions:new":function(e){e.interaction.resizeAxes=`xy`},"interactions:action-start":function(e){(function(e){var t=e.iEvent,n=e.interaction;if(n.prepared.name===`resize`&&n.prepared.edges){var r=t,i=n.rect;n._rects={start:L({},i),corrected:L({},i),previous:L({},i),delta:{left:0,right:0,width:0,top:0,bottom:0,height:0}},r.edges=n.prepared.edges,r.rect=n._rects.corrected,r.deltaRect=n._rects.delta}})(e),He(e)},"interactions:action-move":function(e){(function(e){var t=e.iEvent,n=e.interaction;if(n.prepared.name===`resize`&&n.prepared.edges){var r=t,i=n.interactable.options.resize.invert,a=i===`reposition`||i===`negate`,o=n.rect,s=n._rects,c=s.start,l=s.corrected,u=s.delta,d=s.previous;if(L(d,l),a){if(L(l,o),i===`reposition`){if(l.top>l.bottom){var f=l.top;l.top=l.bottom,l.bottom=f}if(l.left>l.right){var p=l.left;l.left=l.right,l.right=p}}}else l.top=Math.min(o.top,c.bottom),l.bottom=Math.max(o.bottom,c.top),l.left=Math.min(o.left,c.right),l.right=Math.max(o.right,c.left);for(var m in l.width=l.right-l.left,l.height=l.bottom-l.top,l)u[m]=l[m]-d[m];r.edges=n.prepared.edges,r.rect=l,r.deltaRect=u}})(e),He(e)},"interactions:action-end":function(e){var t=e.iEvent,n=e.interaction;if(n.prepared.name===`resize`&&n.prepared.edges){var r=t;r.edges=n.prepared.edges,r.rect=n._rects.corrected,r.deltaRect=n._rects.delta}},"auto-start:check":function(e){var t=e.interaction,n=e.interactable,r=e.element,i=e.rect,a=e.buttons;if(i){var o=L({},t.coords.cur.page),s=n.options.resize;if(s&&s.enabled&&(!t.pointerIsDown||!/mouse|pointer/.test(t.pointerType)||(a&s.mouseButtons)!=0)){if(S.object(s.edges)){var c={left:!1,right:!1,top:!1,bottom:!1};for(var l in c)c[l]=Ve(l,s.edges[l],o,t._latestPointer.eventTarget,r,i,s.margin||We.defaultMargin);c.left=c.left&&!c.right,c.top=c.top&&!c.bottom,(c.left||c.right||c.top||c.bottom)&&(e.action={name:`resize`,edges:c})}else{var u=s.axis!==`y`&&o.x>i.right-We.defaultMargin,d=s.axis!==`x`&&o.y>i.bottom-We.defaultMargin;(u||d)&&(e.action={name:`resize`,axes:(u?`x`:``)+(d?`y`:``)})}return!e.action&&void 0}}}},defaults:{square:!1,preserveAspectRatio:!1,axis:`xy`,margin:NaN,edges:null,invert:`none`},cursors:null,getCursor:function(e){var t=e.edges,n=e.axis,r=e.name,i=We.cursors,a=null;if(n)a=i[r+n];else if(t){for(var o=``,s=0,c=[`top`,`bottom`,`left`,`right`];s<c.length;s++){var l=c[s];t[l]&&(o+=l)}a=i[o]}return a},filterEventType:function(e){return e.search(`resize`)===0},defaultMargin:null},Ge=We,Ke={id:`actions`,install:function(e){e.usePlugin(Be),e.usePlugin(Ge),e.usePlugin(E),e.usePlugin(Le)}},qe=0,Je={request:function(e){return Ue(e)},cancel:function(e){return K(e)},init:function(e){if(Ue=e.requestAnimationFrame,K=e.cancelAnimationFrame,!Ue)for(var t=[`ms`,`moz`,`webkit`,`o`],n=0;n<t.length;n++){var r=t[n];Ue=e[`${r}RequestAnimationFrame`],K=e[`${r}CancelAnimationFrame`]||e[`${r}CancelRequestAnimationFrame`]}Ue&&=Ue.bind(e),K&&=K.bind(e),Ue||(Ue=function(t){var n=Date.now(),r=Math.max(0,16-(n-qe)),i=e.setTimeout((function(){t(n+r)}),r);return qe=n+r,i},K=function(e){return clearTimeout(e)})}},q={defaults:{enabled:!1,margin:60,container:null,speed:300},now:Date.now,interaction:null,i:0,x:0,y:0,isScrolling:!1,prevTime:0,margin:0,speed:0,start:function(e){q.isScrolling=!0,Je.cancel(q.i),e.autoScroll=q,q.interaction=e,q.prevTime=q.now(),q.i=Je.request(q.scroll)},stop:function(){q.isScrolling=!1,q.interaction&&(q.interaction.autoScroll=null),Je.cancel(q.i)},scroll:function(){var e=q.interaction,t=e.interactable,n=e.element,r=e.prepared.name,i=t.options[r].autoScroll,a=Ye(i.container,t,n),o=q.now(),s=(o-q.prevTime)/1e3,c=i.speed*s;if(c>=1){var l={x:q.x*c,y:q.y*c};if(l.x||l.y){var u=Xe(a);S.window(a)?a.scrollBy(l.x,l.y):a&&(a.scrollLeft+=l.x,a.scrollTop+=l.y);var d=Xe(a),f={x:d.x-u.x,y:d.y-u.y};(f.x||f.y)&&t.fire({type:`autoscroll`,target:n,interactable:t,delta:f,interaction:e,container:a})}q.prevTime=o}q.isScrolling&&(Je.cancel(q.i),q.i=Je.request(q.scroll))},check:function(e,t){return e.options[t].autoScroll?.enabled},onInteractionMove:function(e){var t=e.interaction,n=e.pointer;if(t.interacting()&&q.check(t.interactable,t.prepared.name))if(t.simulation)q.x=q.y=0;else{var r,i,a,o,s=t.interactable,c=t.element,l=t.prepared.name,u=s.options[l].autoScroll,d=Ye(u.container,s,c);if(S.window(d))o=n.clientX<q.margin,r=n.clientY<q.margin,i=n.clientX>d.innerWidth-q.margin,a=n.clientY>d.innerHeight-q.margin;else{var f=re(d);o=n.clientX<f.left+q.margin,r=n.clientY<f.top+q.margin,i=n.clientX>f.right-q.margin,a=n.clientY>f.bottom-q.margin}q.x=i?1:o?-1:0,q.y=a?1:r?-1:0,q.isScrolling||(q.margin=u.margin,q.speed=u.speed,q.start(t))}}};function Ye(e,t,n){return(S.string(e)?se(e,t,n):e)||y(n)}function Xe(e){return S.window(e)&&(e=window.document.body),{x:e.scrollLeft,y:e.scrollTop}}var Ze={id:`auto-scroll`,install:function(e){var t=e.defaults,n=e.actions;e.autoScroll=q,q.now=function(){return e.now()},n.phaselessTypes.autoscroll=!0,t.perAction.autoScroll=q.defaults},listeners:{"interactions:new":function(e){e.interaction.autoScroll=null},"interactions:destroy":function(e){e.interaction.autoScroll=null,q.stop(),q.interaction&&=null},"interactions:stop":q.stop,"interactions:action-move":function(e){return q.onInteractionMove(e)}}};function Qe(e,t){var n=!1;return function(){return n||=(_.console.warn(t),!0),e.apply(this,arguments)}}function $e(e,t){return e.name=t.name,e.axis=t.axis,e.edges=t.edges,e}function J(e){return S.bool(e)?(this.options.styleCursor=e,this):e===null?(delete this.options.styleCursor,this):this.options.styleCursor}function et(e){return S.func(e)?(this.options.actionChecker=e,this):e===null?(delete this.options.actionChecker,this):this.options.actionChecker}var tt={id:`auto-start/interactableMethods`,install:function(e){var t=e.Interactable;t.prototype.getAction=function(t,n,r,i){var a=function(e,t,n,r,i){var a={action:null,interactable:e,interaction:n,element:r,rect:e.getRect(r),buttons:t.buttons||{0:1,1:4,3:8,4:16}[t.button]};return i.fire(`auto-start:check`,a),a.action}(this,n,r,i,e);return this.options.actionChecker?this.options.actionChecker(t,n,a,this,i,r):a},t.prototype.ignoreFrom=Qe((function(e){return this._backCompatOption(`ignoreFrom`,e)}),`Interactable.ignoreFrom() has been deprecated. Use Interactble.draggable({ignoreFrom: newValue}).`),t.prototype.allowFrom=Qe((function(e){return this._backCompatOption(`allowFrom`,e)}),`Interactable.allowFrom() has been deprecated. Use Interactble.draggable({allowFrom: newValue}).`),t.prototype.actionChecker=et,t.prototype.styleCursor=J}};function nt(e,t,n,r,i){return t.testIgnoreAllow(t.options[e.name],n,r)&&t.options[e.name].enabled&&ot(t,n,e,i)?e:null}function rt(e,t,n,r,i,a,o){for(var s=0,c=r.length;s<c;s++){var l=r[s],u=i[s],d=l.getAction(t,n,e,u);if(d){var f=nt(d,l,u,a,o);if(f)return{action:f,interactable:l,element:u}}}return{action:null,interactable:null,element:null}}function it(e,t,n,r,i){var a=[],o=[],s=r;function c(e){a.push(e),o.push(s)}for(;S.element(s);){a=[],o=[],i.interactables.forEachMatch(s,c);var l=rt(e,t,n,a,o,r,i);if(l.action&&!l.interactable.options[l.action.name].manualStart)return l;s=P(s)}return{action:null,interactable:null,element:null}}function at(e,t,n){var r=t.action,i=t.interactable,a=t.element;r||={name:null},e.interactable=i,e.element=a,$e(e.prepared,r),e.rect=i&&r.name?i.getRect(a):null,lt(e,n),n.fire(`autoStart:prepared`,{interaction:e})}function ot(e,t,n,r){var i=e.options,a=i[n.name].max,o=i[n.name].maxPerElement,s=r.autoStart.maxInteractions,c=0,l=0,u=0;if(!(a&&o&&s))return!1;for(var d=0,f=r.interactions.list;d<f.length;d++){var p=f[d],m=p.prepared.name;if(p.interacting()&&(++c>=s||p.interactable===e&&((l+=m===n.name?1:0)>=a||p.element===t&&(u++,m===n.name&&u>=o))))return!1}return s>0}function st(e,t){return S.number(e)?(t.autoStart.maxInteractions=e,this):t.autoStart.maxInteractions}function ct(e,t,n){var r=n.autoStart.cursorElement;r&&r!==e&&(r.style.cursor=``),e.ownerDocument.documentElement.style.cursor=t,e.style.cursor=t,n.autoStart.cursorElement=t?e:null}function lt(e,t){var n=e.interactable,r=e.element,i=e.prepared;if(e.pointerType===`mouse`&&n&&n.options.styleCursor){var a=``;if(i.name){var o=n.options[i.name].cursorChecker;a=S.func(o)?o(i,n,r,e._interacting):t.actions.map[i.name].getCursor(i)}ct(e.element,a||``,t)}else t.autoStart.cursorElement&&ct(t.autoStart.cursorElement,``,t)}var ut={id:`auto-start/base`,before:[`actions`],install:function(e){var t=e.interactStatic,n=e.defaults;e.usePlugin(tt),n.base.actionChecker=null,n.base.styleCursor=!0,L(n.perAction,{manualStart:!1,max:1/0,maxPerElement:1,allowFrom:null,ignoreFrom:null,mouseButtons:1}),t.maxInteractions=function(t){return st(t,e)},e.autoStart={maxInteractions:1/0,withinInteractionLimit:ot,cursorElement:null}},listeners:{"interactions:down":function(e,t){var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget;n.interacting()||at(n,it(n,r,i,a,t),t)},"interactions:move":function(e,t){(function(e,t){var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget;n.pointerType!==`mouse`||n.pointerIsDown||n.interacting()||at(n,it(n,r,i,a,t),t)})(e,t),function(e,t){var n=e.interaction;if(n.pointerIsDown&&!n.interacting()&&n.pointerWasMoved&&n.prepared.name){t.fire(`autoStart:before-start`,e);var r=n.interactable,i=n.prepared.name;i&&r&&(r.options[i].manualStart||!ot(r,n.element,n.prepared,t)?n.stop():(n.start(n.prepared,r,n.element),lt(n,t)))}}(e,t)},"interactions:stop":function(e,t){var n=e.interaction,r=n.interactable;r&&r.options.styleCursor&&ct(n.element,``,t)}},maxInteractions:st,withinInteractionLimit:ot,validateAction:nt},dt={id:`auto-start/dragAxis`,listeners:{"autoStart:before-start":function(e,t){var n=e.interaction,r=e.eventTarget,i=e.dx,a=e.dy;if(n.prepared.name===`drag`){var o=Math.abs(i),s=Math.abs(a),c=n.interactable.options.drag,l=c.startAxis,u=o>s?`x`:o<s?`y`:`xy`;if(n.prepared.axis=c.lockAxis===`start`?u[0]:c.lockAxis,u!==`xy`&&l!==`xy`&&l!==u){n.prepared.name=null;for(var d=r,f=function(e){if(e!==n.interactable){var i=n.interactable.options.drag;if(!i.manualStart&&e.testIgnoreAllow(i,d,r)){var a=e.getAction(n.downPointer,n.downEvent,n,d);if(a&&a.name===`drag`&&function(e,t){if(!t)return!1;var n=t.options.drag.startAxis;return e===`xy`||n===`xy`||n===e}(u,e)&&ut.validateAction(a,e,d,r,t))return e}}};S.element(d);){var p=t.interactables.forEachMatch(d,f);if(p){n.prepared.name=`drag`,n.interactable=p,n.element=d;break}d=P(d)}}}}}};function ft(e){var t=e.prepared&&e.prepared.name;if(!t)return null;var n=e.interactable.options;return n[t].hold||n[t].delay}var pt={id:`auto-start/hold`,install:function(e){var t=e.defaults;e.usePlugin(ut),t.perAction.hold=0,t.perAction.delay=0},listeners:{"interactions:new":function(e){e.interaction.autoStartHoldTimer=null},"autoStart:prepared":function(e){var t=e.interaction,n=ft(t);n>0&&(t.autoStartHoldTimer=setTimeout((function(){t.start(t.prepared,t.interactable,t.element)}),n))},"interactions:move":function(e){var t=e.interaction,n=e.duplicate;t.autoStartHoldTimer&&t.pointerWasMoved&&!n&&(clearTimeout(t.autoStartHoldTimer),t.autoStartHoldTimer=null)},"autoStart:before-start":function(e){var t=e.interaction;ft(t)>0&&(t.prepared.name=null)}},getHoldDuration:ft},mt={id:`auto-start`,install:function(e){e.usePlugin(ut),e.usePlugin(pt),e.usePlugin(dt)}},ht=function(e){return/^(always|never|auto)$/.test(e)?(this.options.preventDefault=e,this):S.bool(e)?(this.options.preventDefault=e?`always`:`never`,this):this.options.preventDefault};function gt(e){var t=e.interaction,n=e.event;t.interactable&&t.interactable.checkAndPreventDefault(n)}var _t={id:`core/interactablePreventDefault`,install:function(e){var t=e.Interactable;t.prototype.preventDefault=ht,t.prototype.checkAndPreventDefault=function(t){return function(e,t,n){var r=e.options.preventDefault;if(r!==`never`)if(r!==`always`){if(t.events.supportsPassive&&/^touch(start|move)$/.test(n.type)){var i=y(n.target).document,a=t.getDocOptions(i);if(!a||!a.events||!1!==a.events.passive)return}/^(mouse|pointer|touch)*(down|start)/i.test(n.type)||S.element(n.target)&&F(n.target,`input,select,textarea,[contenteditable=true],[contenteditable=true] *`)||n.preventDefault()}else n.preventDefault()}(this,e,t)},e.interactions.docEvents.push({type:`dragstart`,listener:function(t){for(var n=0,r=e.interactions.list;n<r.length;n++){var i=r[n];if(i.element&&(i.element===t.target||M(i.element,t.target)))return void i.interactable.checkAndPreventDefault(t)}}})},listeners:[`down`,`move`,`up`,`cancel`].reduce((function(e,t){return e[`interactions:${t}`]=gt,e}),{})};function vt(e,t){if(t.phaselessTypes[e])return!0;for(var n in t.map)if(e.indexOf(n)===0&&e.substr(n.length)in t.phases)return!0;return!1}function yt(e){var t={};for(var n in e){var r=e[n];S.plainObject(r)?t[n]=yt(r):S.array(r)?t[n]=Ee(r):t[n]=r}return t}var bt=function(){function e(t){i(this,e),this.states=[],this.startOffset={left:0,right:0,top:0,bottom:0},this.startDelta=void 0,this.result=void 0,this.endResult=void 0,this.startEdges=void 0,this.edges=void 0,this.interaction=void 0,this.interaction=t,this.result=xt(),this.edges={left:!1,right:!1,top:!1,bottom:!1}}return o(e,[{key:`start`,value:function(e,t){var n,r,i=e.phase,a=this.interaction,o=function(e){var t=e.interactable.options[e.prepared.name],n=t.modifiers;return n&&n.length?n:[`snap`,`snapSize`,`snapEdges`,`restrict`,`restrictEdges`,`restrictSize`].map((function(e){var n=t[e];return n&&n.enabled&&{options:n,methods:n._methods}})).filter((function(e){return!!e}))}(a);this.prepareStates(o),this.startEdges=L({},a.edges),this.edges=L({},this.startEdges),this.startOffset=(n=a.rect,r=t,n?{left:r.x-n.left,top:r.y-n.top,right:n.right-r.x,bottom:n.bottom-r.y}:{left:0,top:0,right:0,bottom:0}),this.startDelta={x:0,y:0};var s=this.fillArg({phase:i,pageCoords:t,preEnd:!1});return this.result=xt(),this.startAll(s),this.result=this.setAll(s)}},{key:`fillArg`,value:function(e){var t=this.interaction;return e.interaction=t,e.interactable=t.interactable,e.element=t.element,e.rect||=t.rect,e.edges||=this.startEdges,e.startOffset=this.startOffset,e}},{key:`startAll`,value:function(e){for(var t=0,n=this.states;t<n.length;t++){var r=n[t];r.methods.start&&(e.state=r,r.methods.start(e))}}},{key:`setAll`,value:function(e){var t=e.phase,n=e.preEnd,r=e.skipModifiers,i=e.rect,a=e.edges;e.coords=L({},e.pageCoords),e.rect=L({},i),e.edges=L({},a);for(var o=r?this.states.slice(r):this.states,s=xt(e.coords,e.rect),c=0;c<o.length;c++){var l,u=o[c],d=u.options,f=L({},e.coords),p=null;(l=u.methods)!=null&&l.set&&this.shouldDo(d,n,t)&&(e.state=u,p=u.methods.set(e),z(e.edges,e.rect,{x:e.coords.x-f.x,y:e.coords.y-f.y})),s.eventProps.push(p)}L(this.edges,e.edges),s.delta.x=e.coords.x-e.pageCoords.x,s.delta.y=e.coords.y-e.pageCoords.y,s.rectDelta.left=e.rect.left-i.left,s.rectDelta.right=e.rect.right-i.right,s.rectDelta.top=e.rect.top-i.top,s.rectDelta.bottom=e.rect.bottom-i.bottom;var m=this.result.coords,h=this.result.rect;return m&&h&&(s.changed=s.rect.left!==h.left||s.rect.right!==h.right||s.rect.top!==h.top||s.rect.bottom!==h.bottom||m.x!==s.coords.x||m.y!==s.coords.y),s}},{key:`applyToInteraction`,value:function(e){var t=this.interaction,n=e.phase,r=t.coords.cur,i=t.coords.start,a=this.result,o=this.startDelta,s=a.delta;n===`start`&&L(this.startDelta,a.delta);for(var c=0,l=[[i,o],[r,s]];c<l.length;c++){var u=l[c],d=u[0],f=u[1];d.page.x+=f.x,d.page.y+=f.y,d.client.x+=f.x,d.client.y+=f.y}var p=this.result.rectDelta,m=e.rect||t.rect;m.left+=p.left,m.right+=p.right,m.top+=p.top,m.bottom+=p.bottom,m.width=m.right-m.left,m.height=m.bottom-m.top}},{key:`setAndApply`,value:function(e){var t=this.interaction,n=e.phase,r=e.preEnd,i=e.skipModifiers,a=this.setAll(this.fillArg({preEnd:r,phase:n,pageCoords:e.modifiedCoords||t.coords.cur.page}));if(this.result=a,!a.changed&&(!i||i<this.states.length)&&t.interacting())return!1;if(e.modifiedCoords){var o=t.coords.cur.page,s={x:e.modifiedCoords.x-o.x,y:e.modifiedCoords.y-o.y};a.coords.x+=s.x,a.coords.y+=s.y,a.delta.x+=s.x,a.delta.y+=s.y}this.applyToInteraction(e)}},{key:`beforeEnd`,value:function(e){var t=e.interaction,n=e.event,r=this.states;if(r&&r.length){for(var i=!1,a=0;a<r.length;a++){var o=r[a];e.state=o;var s=o.options,c=o.methods,l=c.beforeEnd&&c.beforeEnd(e);if(l)return this.endResult=l,!1;i||=!i&&this.shouldDo(s,!0,e.phase,!0)}i&&t.move({event:n,preEnd:!0})}}},{key:`stop`,value:function(e){var t=e.interaction;if(this.states&&this.states.length){var n=L({states:this.states,interactable:t.interactable,element:t.element,rect:null},e);this.fillArg(n);for(var r=0,i=this.states;r<i.length;r++){var a=i[r];n.state=a,a.methods.stop&&a.methods.stop(n)}this.states=null,this.endResult=null}}},{key:`prepareStates`,value:function(e){this.states=[];for(var t=0;t<e.length;t++){var n=e[t],r=n.options,i=n.methods,a=n.name;this.states.push({options:r,methods:i,index:t,name:a})}return this.states}},{key:`restoreInteractionCoords`,value:function(e){var t=e.interaction,n=t.coords,r=t.rect,i=t.modification;if(i.result){for(var a=i.startDelta,o=i.result,s=o.delta,c=o.rectDelta,l=0,u=[[n.start,a],[n.cur,s]];l<u.length;l++){var d=u[l],f=d[0],p=d[1];f.page.x-=p.x,f.page.y-=p.y,f.client.x-=p.x,f.client.y-=p.y}r.left-=c.left,r.right-=c.right,r.top-=c.top,r.bottom-=c.bottom}}},{key:`shouldDo`,value:function(e,t,n,r){return!(!e||!1===e.enabled||r&&!e.endOnly||e.endOnly&&!t||n===`start`&&!e.setStart)}},{key:`copyFrom`,value:function(e){this.startOffset=e.startOffset,this.startDelta=e.startDelta,this.startEdges=e.startEdges,this.edges=e.edges,this.states=e.states.map((function(e){return yt(e)})),this.result=xt(L({},e.result.coords),L({},e.result.rect))}},{key:`destroy`,value:function(){for(var e in this)this[e]=null}}]),e}();function xt(e,t){return{rect:t,coords:e,delta:{x:0,y:0},rectDelta:{left:0,right:0,top:0,bottom:0},eventProps:[],changed:!0}}function St(e,t){var n=e.defaults,r={start:e.start,set:e.set,beforeEnd:e.beforeEnd,stop:e.stop},i=function(e){var i=e||{};for(var a in i.enabled=!1!==i.enabled,n)a in i||(i[a]=n[a]);var o={options:i,methods:r,name:t,enable:function(){return i.enabled=!0,o},disable:function(){return i.enabled=!1,o}};return o};return t&&typeof t==`string`&&(i._defaults=n,i._methods=r),i}function Ct(e){var t=e.iEvent,n=e.interaction.modification.result;n&&(t.modifiers=n.eventProps)}var wt={id:`modifiers/base`,before:[`actions`],install:function(e){e.defaults.perAction.modifiers=[]},listeners:{"interactions:new":function(e){var t=e.interaction;t.modification=new bt(t)},"interactions:before-action-start":function(e){var t=e.interaction,n=e.interaction.modification;n.start(e,t.coords.start.page),t.edges=n.edges,n.applyToInteraction(e)},"interactions:before-action-move":function(e){var t=e.interaction,n=t.modification,r=n.setAndApply(e);return t.edges=n.edges,r},"interactions:before-action-end":function(e){var t=e.interaction,n=t.modification,r=n.beforeEnd(e);return t.edges=n.startEdges,r},"interactions:action-start":Ct,"interactions:action-move":Ct,"interactions:action-end":Ct,"interactions:after-action-start":function(e){return e.interaction.modification.restoreInteractionCoords(e)},"interactions:after-action-move":function(e){return e.interaction.modification.restoreInteractionCoords(e)},"interactions:stop":function(e){return e.interaction.modification.stop(e)}}},Tt={base:{preventDefault:`auto`,deltaSource:`page`},perAction:{enabled:!1,origin:{x:0,y:0}},actions:{}},Et=function(e){c(n,e);var t=f(n);function n(e,r,a,o,s,c,l){var u;i(this,n),(u=t.call(this,e)).relatedTarget=null,u.screenX=void 0,u.screenY=void 0,u.button=void 0,u.buttons=void 0,u.ctrlKey=void 0,u.shiftKey=void 0,u.altKey=void 0,u.metaKey=void 0,u.page=void 0,u.client=void 0,u.delta=void 0,u.rect=void 0,u.x0=void 0,u.y0=void 0,u.t0=void 0,u.dt=void 0,u.duration=void 0,u.clientX0=void 0,u.clientY0=void 0,u.velocity=void 0,u.speed=void 0,u.swipe=void 0,u.axes=void 0,u.preEnd=void 0,s||=e.element;var f=e.interactable,p=(f&&f.options||Tt).deltaSource,m=B(f,s,a),h=o===`start`,g=o===`end`,_=h?d(u):e.prevEvent,v=h?e.coords.start:g?{page:_.page,client:_.client,timeStamp:e.coords.cur.timeStamp}:e.coords.cur;return u.page=L({},v.page),u.client=L({},v.client),u.rect=L({},e.rect),u.timeStamp=v.timeStamp,g||(u.page.x-=m.x,u.page.y-=m.y,u.client.x-=m.x,u.client.y-=m.y),u.ctrlKey=r.ctrlKey,u.altKey=r.altKey,u.shiftKey=r.shiftKey,u.metaKey=r.metaKey,u.button=r.button,u.buttons=r.buttons,u.target=s,u.currentTarget=s,u.preEnd=c,u.type=l||a+(o||``),u.interactable=f,u.t0=h?e.pointers[e.pointers.length-1].downTime:_.t0,u.x0=e.coords.start.page.x-m.x,u.y0=e.coords.start.page.y-m.y,u.clientX0=e.coords.start.client.x-m.x,u.clientY0=e.coords.start.client.y-m.y,u.delta=h||g?{x:0,y:0}:{x:u[p].x-_[p].x,y:u[p].y-_[p].y},u.dt=e.coords.delta.timeStamp,u.duration=u.timeStamp-u.t0,u.velocity=L({},e.coords.velocity[p]),u.speed=U(u.velocity.x,u.velocity.y),u.swipe=g||o===`inertiastart`?u.getSwipe():null,u}return o(n,[{key:`getSwipe`,value:function(){var e=this._interaction;if(e.prevEvent.speed<600||this.timeStamp-e.prevEvent.timeStamp>150)return null;var t=180*Math.atan2(e.prevEvent.velocityY,e.prevEvent.velocityX)/Math.PI;t<0&&(t+=360);var n=112.5<=t&&t<247.5,r=202.5<=t&&t<337.5;return{up:r,down:!r&&22.5<=t&&t<157.5,left:n,right:!n&&(292.5<=t||t<67.5),angle:t,speed:e.prevEvent.speed,velocity:{x:e.prevEvent.velocityX,y:e.prevEvent.velocityY}}}},{key:`preventDefault`,value:function(){}},{key:`stopImmediatePropagation`,value:function(){this.immediatePropagationStopped=this.propagationStopped=!0}},{key:`stopPropagation`,value:function(){this.propagationStopped=!0}}]),n}(we);Object.defineProperties(Et.prototype,{pageX:{get:function(){return this.page.x},set:function(e){this.page.x=e}},pageY:{get:function(){return this.page.y},set:function(e){this.page.y=e}},clientX:{get:function(){return this.client.x},set:function(e){this.client.x=e}},clientY:{get:function(){return this.client.y},set:function(e){this.client.y=e}},dx:{get:function(){return this.delta.x},set:function(e){this.delta.x=e}},dy:{get:function(){return this.delta.y},set:function(e){this.delta.y=e}},velocityX:{get:function(){return this.velocity.x},set:function(e){this.velocity.x=e}},velocityY:{get:function(){return this.velocity.y},set:function(e){this.velocity.y=e}}});var Dt=o((function e(t,n,r,a,o){i(this,e),this.id=void 0,this.pointer=void 0,this.event=void 0,this.downTime=void 0,this.downTarget=void 0,this.id=t,this.pointer=n,this.event=r,this.downTime=a,this.downTarget=o})),Ot=function(e){return e.interactable=``,e.element=``,e.prepared=``,e.pointerIsDown=``,e.pointerWasMoved=``,e._proxy=``,e}({}),kt=function(e){return e.start=``,e.move=``,e.end=``,e.stop=``,e.interacting=``,e}({}),At=0,jt=function(){function e(t){var n=this,r=t.pointerType,a=t.scopeFire;i(this,e),this.interactable=null,this.element=null,this.rect=null,this._rects=void 0,this.edges=null,this._scopeFire=void 0,this.prepared={name:null,axis:null,edges:null},this.pointerType=void 0,this.pointers=[],this.downEvent=null,this.downPointer={},this._latestPointer={pointer:null,event:null,eventTarget:null},this.prevEvent=null,this.pointerIsDown=!1,this.pointerWasMoved=!1,this._interacting=!1,this._ending=!1,this._stopped=!0,this._proxy=void 0,this.simulation=null,this.doMove=Qe((function(e){this.move(e)}),`The interaction.doMove() method has been renamed to interaction.move()`),this.coords={start:{page:{x:0,y:0},client:{x:0,y:0},timeStamp:0},prev:{page:{x:0,y:0},client:{x:0,y:0},timeStamp:0},cur:{page:{x:0,y:0},client:{x:0,y:0},timeStamp:0},delta:{page:{x:0,y:0},client:{x:0,y:0},timeStamp:0},velocity:{page:{x:0,y:0},client:{x:0,y:0},timeStamp:0}},this._id=At++,this._scopeFire=a,this.pointerType=r;var o=this;this._proxy={};var s=function(e){Object.defineProperty(n._proxy,e,{get:function(){return o[e]}})};for(var c in Ot)s(c);var l=function(e){Object.defineProperty(n._proxy,e,{value:function(){return o[e].apply(o,arguments)}})};for(var u in kt)l(u);this._scopeFire(`interactions:new`,{interaction:this})}return o(e,[{key:`pointerMoveTolerance`,get:function(){return 1}},{key:`pointerDown`,value:function(e,t,n){var r=this.updatePointer(e,t,n,!0),i=this.pointers[r];this._scopeFire(`interactions:down`,{pointer:e,event:t,eventTarget:n,pointerIndex:r,pointerInfo:i,type:`down`,interaction:this})}},{key:`start`,value:function(e,t,n){return!(this.interacting()||!this.pointerIsDown||this.pointers.length<(e.name===`gesture`?2:1)||!t.options[e.name].enabled)&&($e(this.prepared,e),this.interactable=t,this.element=n,this.rect=t.getRect(n),this.edges=this.prepared.edges?L({},this.prepared.edges):{left:!0,right:!0,top:!0,bottom:!0},this._stopped=!1,this._interacting=this._doPhase({interaction:this,event:this.downEvent,phase:`start`})&&!this._stopped,this._interacting)}},{key:`pointerMove`,value:function(e,t,n){this.simulation||this.modification&&this.modification.endResult||this.updatePointer(e,t,n,!1);var r,i,a=this.coords.cur.page.x===this.coords.prev.page.x&&this.coords.cur.page.y===this.coords.prev.page.y&&this.coords.cur.client.x===this.coords.prev.client.x&&this.coords.cur.client.y===this.coords.prev.client.y;this.pointerIsDown&&!this.pointerWasMoved&&(r=this.coords.cur.client.x-this.coords.start.client.x,i=this.coords.cur.client.y-this.coords.start.client.y,this.pointerWasMoved=U(r,i)>this.pointerMoveTolerance);var o,s,c,l=this.getPointerIndex(e),u={pointer:e,pointerIndex:l,pointerInfo:this.pointers[l],event:t,type:`move`,eventTarget:n,dx:r,dy:i,duplicate:a,interaction:this};a||(o=this.coords.velocity,s=this.coords.delta,c=Math.max(s.timeStamp/1e3,.001),o.page.x=s.page.x/c,o.page.y=s.page.y/c,o.client.x=s.client.x/c,o.client.y=s.client.y/c,o.timeStamp=c),this._scopeFire(`interactions:move`,u),a||this.simulation||(this.interacting()&&(u.type=null,this.move(u)),this.pointerWasMoved&&de(this.coords.prev,this.coords.cur))}},{key:`move`,value:function(e){e&&e.event||fe(this.coords.delta),(e=L({pointer:this._latestPointer.pointer,event:this._latestPointer.event,eventTarget:this._latestPointer.eventTarget,interaction:this},e||{})).phase=`move`,this._doPhase(e)}},{key:`pointerUp`,value:function(e,t,n,r){var i=this.getPointerIndex(e);i===-1&&(i=this.updatePointer(e,t,n,!1));var a=/cancel$/i.test(t.type)?`cancel`:`up`;this._scopeFire(`interactions:${a}`,{pointer:e,pointerIndex:i,pointerInfo:this.pointers[i],event:t,eventTarget:n,type:a,curEventTarget:r,interaction:this}),this.simulation||this.end(t),this.removePointer(e,t)}},{key:`documentBlur`,value:function(e){this.end(e),this._scopeFire(`interactions:blur`,{event:e,type:`blur`,interaction:this})}},{key:`end`,value:function(e){var t;this._ending=!0,e||=this._latestPointer.event,this.interacting()&&(t=this._doPhase({event:e,interaction:this,phase:`end`})),this._ending=!1,!0===t&&this.stop()}},{key:`currentAction`,value:function(){return this._interacting?this.prepared.name:null}},{key:`interacting`,value:function(){return this._interacting}},{key:`stop`,value:function(){this._scopeFire(`interactions:stop`,{interaction:this}),this.interactable=this.element=null,this._interacting=!1,this._stopped=!0,this.prepared.name=this.prevEvent=null}},{key:`getPointerIndex`,value:function(e){var t=ge(e);return this.pointerType===`mouse`||this.pointerType===`pen`?this.pointers.length-1:De(this.pointers,(function(e){return e.id===t}))}},{key:`getPointerInfo`,value:function(e){return this.pointers[this.getPointerIndex(e)]}},{key:`updatePointer`,value:function(e,t,n,r){var i,a,o,s=ge(e),c=this.getPointerIndex(e),l=this.pointers[c];return r=!1!==r&&(r||/(down|start)$/i.test(t.type)),l?l.pointer=e:(l=new Dt(s,e,t,null,null),c=this.pointers.length,this.pointers.push(l)),_e(this.coords.cur,this.pointers.map((function(e){return e.pointer})),this._now()),i=this.coords.delta,a=this.coords.prev,o=this.coords.cur,i.page.x=o.page.x-a.page.x,i.page.y=o.page.y-a.page.y,i.client.x=o.client.x-a.client.x,i.client.y=o.client.y-a.client.y,i.timeStamp=o.timeStamp-a.timeStamp,r&&(this.pointerIsDown=!0,l.downTime=this.coords.cur.timeStamp,l.downTarget=n,ue(this.downPointer,e),this.interacting()||(de(this.coords.start,this.coords.cur),de(this.coords.prev,this.coords.cur),this.downEvent=t,this.pointerWasMoved=!1)),this._updateLatestPointer(e,t,n),this._scopeFire(`interactions:update-pointer`,{pointer:e,event:t,eventTarget:n,down:r,pointerInfo:l,pointerIndex:c,interaction:this}),c}},{key:`removePointer`,value:function(e,t){var n=this.getPointerIndex(e);if(n!==-1){var r=this.pointers[n];this._scopeFire(`interactions:remove-pointer`,{pointer:e,event:t,eventTarget:null,pointerIndex:n,pointerInfo:r,interaction:this}),this.pointers.splice(n,1),this.pointerIsDown=!1}}},{key:`_updateLatestPointer`,value:function(e,t,n){this._latestPointer.pointer=e,this._latestPointer.event=t,this._latestPointer.eventTarget=n}},{key:`destroy`,value:function(){this._latestPointer.pointer=null,this._latestPointer.event=null,this._latestPointer.eventTarget=null}},{key:`_createPreparedEvent`,value:function(e,t,n,r){return new Et(this,e,this.prepared.name,t,this.element,n,r)}},{key:`_fireEvent`,value:function(e){var t;(t=this.interactable)==null||t.fire(e),(!this.prevEvent||e.timeStamp>=this.prevEvent.timeStamp)&&(this.prevEvent=e)}},{key:`_doPhase`,value:function(e){var t=e.event,n=e.phase,r=e.preEnd,i=e.type,a=this.rect;if(a&&n===`move`&&(z(this.edges,a,this.coords.delta[this.interactable.options.deltaSource]),a.width=a.right-a.left,a.height=a.bottom-a.top),!1===this._scopeFire(`interactions:before-action-${n}`,e))return!1;var o=e.iEvent=this._createPreparedEvent(t,n,r,i);return this._scopeFire(`interactions:action-${n}`,e),n===`start`&&(this.prevEvent=o),this._fireEvent(o),this._scopeFire(`interactions:after-action-${n}`,e),!0}},{key:`_now`,value:function(){return Date.now()}}]),e}();function Mt(e){Nt(e.interaction)}function Nt(e){if(!function(e){return!(!e.offset.pending.x&&!e.offset.pending.y)}(e))return!1;var t=e.offset.pending;return Ft(e.coords.cur,t),Ft(e.coords.delta,t),z(e.edges,e.rect,t),t.x=0,t.y=0,!0}function Pt(e){var t=e.x,n=e.y;this.offset.pending.x+=t,this.offset.pending.y+=n,this.offset.total.x+=t,this.offset.total.y+=n}function Ft(e,t){var n=e.page,r=e.client,i=t.x,a=t.y;n.x+=i,n.y+=a,r.x+=i,r.y+=a}kt.offsetBy=``;var It={id:`offset`,before:[`modifiers`,`pointer-events`,`actions`,`inertia`],install:function(e){e.Interaction.prototype.offsetBy=Pt},listeners:{"interactions:new":function(e){e.interaction.offset={total:{x:0,y:0},pending:{x:0,y:0}}},"interactions:update-pointer":function(e){return function(e){e.pointerIsDown&&(Ft(e.coords.cur,e.offset.total),e.offset.pending.x=0,e.offset.pending.y=0)}(e.interaction)},"interactions:before-action-start":Mt,"interactions:before-action-move":Mt,"interactions:before-action-end":function(e){var t=e.interaction;if(Nt(t))return t.move({offset:!0}),t.end(),!1},"interactions:stop":function(e){var t=e.interaction;t.offset.total.x=0,t.offset.total.y=0,t.offset.pending.x=0,t.offset.pending.y=0}}},Lt=function(){function e(t){i(this,e),this.active=!1,this.isModified=!1,this.smoothEnd=!1,this.allowResume=!1,this.modification=void 0,this.modifierCount=0,this.modifierArg=void 0,this.startCoords=void 0,this.t0=0,this.v0=0,this.te=0,this.targetOffset=void 0,this.modifiedOffset=void 0,this.currentOffset=void 0,this.lambda_v0=0,this.one_ve_v0=0,this.timeout=void 0,this.interaction=void 0,this.interaction=t}return o(e,[{key:`start`,value:function(e){var t=this.interaction,n=Rt(t);if(!n||!n.enabled)return!1;var r=t.coords.velocity.client,i=U(r.x,r.y),a=this.modification||=new bt(t);if(a.copyFrom(t.modification),this.t0=t._now(),this.allowResume=n.allowResume,this.v0=i,this.currentOffset={x:0,y:0},this.startCoords=t.coords.cur.page,this.modifierArg=a.fillArg({pageCoords:this.startCoords,preEnd:!0,phase:`inertiastart`}),this.t0-t.coords.cur.timeStamp<50&&i>n.minSpeed&&i>n.endSpeed)this.startInertia();else{if(a.result=a.setAll(this.modifierArg),!a.result.changed)return!1;this.startSmoothEnd()}return t.modification.result.rect=null,t.offsetBy(this.targetOffset),t._doPhase({interaction:t,event:e,phase:`inertiastart`}),t.offsetBy({x:-this.targetOffset.x,y:-this.targetOffset.y}),t.modification.result.rect=null,this.active=!0,t.simulation=this,!0}},{key:`startInertia`,value:function(){var e=this,t=this.interaction.coords.velocity.client,n=Rt(this.interaction),r=n.resistance,i=-Math.log(n.endSpeed/this.v0)/r;this.targetOffset={x:(t.x-i)/r,y:(t.y-i)/r},this.te=i,this.lambda_v0=r/this.v0,this.one_ve_v0=1-n.endSpeed/this.v0;var a=this.modification,o=this.modifierArg;o.pageCoords={x:this.startCoords.x+this.targetOffset.x,y:this.startCoords.y+this.targetOffset.y},a.result=a.setAll(o),a.result.changed&&(this.isModified=!0,this.modifiedOffset={x:this.targetOffset.x+a.result.delta.x,y:this.targetOffset.y+a.result.delta.y}),this.onNextFrame((function(){return e.inertiaTick()}))}},{key:`startSmoothEnd`,value:function(){var e=this;this.smoothEnd=!0,this.isModified=!0,this.targetOffset={x:this.modification.result.delta.x,y:this.modification.result.delta.y},this.onNextFrame((function(){return e.smoothEndTick()}))}},{key:`onNextFrame`,value:function(e){var t=this;this.timeout=Je.request((function(){t.active&&e()}))}},{key:`inertiaTick`,value:function(){var e,t,n,r,i,a,o,s=this,c=this.interaction,l=Rt(c).resistance,u=(c._now()-this.t0)/1e3;if(u<this.te){var d,f=1-(Math.exp(-l*u)-this.lambda_v0)/this.one_ve_v0;this.isModified?(e=0,t=0,n=this.targetOffset.x,r=this.targetOffset.y,i=this.modifiedOffset.x,a=this.modifiedOffset.y,d={x:Bt(o=f,e,n,i),y:Bt(o,t,r,a)}):d={x:this.targetOffset.x*f,y:this.targetOffset.y*f};var p={x:d.x-this.currentOffset.x,y:d.y-this.currentOffset.y};this.currentOffset.x+=p.x,this.currentOffset.y+=p.y,c.offsetBy(p),c.move(),this.onNextFrame((function(){return s.inertiaTick()}))}else c.offsetBy({x:this.modifiedOffset.x-this.currentOffset.x,y:this.modifiedOffset.y-this.currentOffset.y}),this.end()}},{key:`smoothEndTick`,value:function(){var e=this,t=this.interaction,n=t._now()-this.t0,r=Rt(t).smoothEndDuration;if(n<r){var i={x:Vt(n,0,this.targetOffset.x,r),y:Vt(n,0,this.targetOffset.y,r)},a={x:i.x-this.currentOffset.x,y:i.y-this.currentOffset.y};this.currentOffset.x+=a.x,this.currentOffset.y+=a.y,t.offsetBy(a),t.move({skipModifiers:this.modifierCount}),this.onNextFrame((function(){return e.smoothEndTick()}))}else t.offsetBy({x:this.targetOffset.x-this.currentOffset.x,y:this.targetOffset.y-this.currentOffset.y}),this.end()}},{key:`resume`,value:function(e){var t=e.pointer,n=e.event,r=e.eventTarget,i=this.interaction;i.offsetBy({x:-this.currentOffset.x,y:-this.currentOffset.y}),i.updatePointer(t,n,r,!0),i._doPhase({interaction:i,event:n,phase:`resume`}),de(i.coords.prev,i.coords.cur),this.stop()}},{key:`end`,value:function(){this.interaction.move(),this.interaction.end(),this.stop()}},{key:`stop`,value:function(){this.active=this.smoothEnd=!1,this.interaction.simulation=null,Je.cancel(this.timeout)}}]),e}();function Rt(e){var t=e.interactable,n=e.prepared;return t&&t.options&&n.name&&t.options[n.name].inertia}var zt={id:`inertia`,before:[`modifiers`,`actions`],install:function(e){var t=e.defaults;e.usePlugin(It),e.usePlugin(wt),e.actions.phases.inertiastart=!0,e.actions.phases.resume=!0,t.perAction.inertia={enabled:!1,resistance:10,minSpeed:100,endSpeed:10,allowResume:!0,smoothEndDuration:300}},listeners:{"interactions:new":function(e){var t=e.interaction;t.inertia=new Lt(t)},"interactions:before-action-end":function(e){var t=e.interaction,n=e.event;return(!t._interacting||t.simulation||!t.inertia.start(n))&&null},"interactions:down":function(e){var t=e.interaction,n=e.eventTarget,r=t.inertia;if(r.active)for(var i=n;S.element(i);){if(i===t.element){r.resume(e);break}i=P(i)}},"interactions:stop":function(e){var t=e.interaction.inertia;t.active&&t.stop()},"interactions:before-action-resume":function(e){var t=e.interaction.modification;t.stop(e),t.start(e,e.interaction.coords.cur.page),t.applyToInteraction(e)},"interactions:before-action-inertiastart":function(e){return e.interaction.modification.setAndApply(e)},"interactions:action-resume":Ct,"interactions:action-inertiastart":Ct,"interactions:after-action-inertiastart":function(e){return e.interaction.modification.restoreInteractionCoords(e)},"interactions:after-action-resume":function(e){return e.interaction.modification.restoreInteractionCoords(e)}}};function Bt(e,t,n,r){var i=1-e;return i*i*t+2*i*e*n+e*e*r}function Vt(e,t,n,r){return-n*(e/=r)*(e-2)+t}var Ht=zt;function Ut(e,t){for(var n=0;n<t.length;n++){var r=t[n];if(e.immediatePropagationStopped)break;r(e)}}var Wt=function(){function e(t){i(this,e),this.options=void 0,this.types={},this.propagationStopped=!1,this.immediatePropagationStopped=!1,this.global=void 0,this.options=L({},t||{})}return o(e,[{key:`fire`,value:function(e){var t,n=this.global;(t=this.types[e.type])&&Ut(e,t),!e.propagationStopped&&n&&(t=n[e.type])&&Ut(e,t)}},{key:`on`,value:function(e,t){var n=V(e,t);for(e in n)this.types[e]=Te(this.types[e]||[],n[e])}},{key:`off`,value:function(e,t){var n=V(e,t);for(e in n){var r=this.types[e];if(r&&r.length)for(var i=0,a=n[e];i<a.length;i++){var o=a[i],s=r.indexOf(o);s!==-1&&r.splice(s,1)}}}},{key:`getRect`,value:function(e){return null}}]),e}(),Gt=function(){function e(t){i(this,e),this.currentTarget=void 0,this.originalEvent=void 0,this.type=void 0,this.originalEvent=t,ue(this,t)}return o(e,[{key:`preventOriginalDefault`,value:function(){this.originalEvent.preventDefault()}},{key:`stopPropagation`,value:function(){this.originalEvent.stopPropagation()}},{key:`stopImmediatePropagation`,value:function(){this.originalEvent.stopImmediatePropagation()}}]),e}();function Kt(e){return S.object(e)?{capture:!!e.capture,passive:!!e.passive}:{capture:!!e,passive:!1}}function qt(e,t){return e===t||(typeof e==`boolean`?!!t.capture===e&&!!t.passive==0:!!e.capture==!!t.capture&&!!e.passive==!!t.passive)}var Jt={id:`events`,install:function(e){var t,n=[],r={},i=[],a={add:o,remove:s,addDelegate:function(e,t,n,a,s){var u=Kt(s);if(!r[n]){r[n]=[];for(var d=0;d<i.length;d++){var f=i[d];o(f,n,c),o(f,n,l,!0)}}var p=r[n],m=Oe(p,(function(n){return n.selector===e&&n.context===t}));m||(m={selector:e,context:t,listeners:[]},p.push(m)),m.listeners.push({func:a,options:u})},removeDelegate:function(e,t,n,i,a){var o,u=Kt(a),d=r[n],f=!1;if(d)for(o=d.length-1;o>=0;o--){var p=d[o];if(p.selector===e&&p.context===t){for(var m=p.listeners,h=m.length-1;h>=0;h--){var g=m[h];if(g.func===i&&qt(g.options,u)){m.splice(h,1),m.length||(d.splice(o,1),s(t,n,c),s(t,n,l,!0)),f=!0;break}}if(f)break}}},delegateListener:c,delegateUseCapture:l,delegatedEvents:r,documents:i,targets:n,supportsOptions:!1,supportsPassive:!1};function o(e,t,r,i){if(e.addEventListener){var o=Kt(i),s=Oe(n,(function(t){return t.eventTarget===e}));s||(s={eventTarget:e,events:{}},n.push(s)),s.events[t]||(s.events[t]=[]),Oe(s.events[t],(function(e){return e.func===r&&qt(e.options,o)}))||(e.addEventListener(t,r,a.supportsOptions?o:o.capture),s.events[t].push({func:r,options:o}))}}function s(e,t,r,i){if(e.addEventListener&&e.removeEventListener){var o=De(n,(function(t){return t.eventTarget===e})),c=n[o];if(c&&c.events)if(t!==`all`){var l=!1,u=c.events[t];if(u){if(r===`all`){for(var d=u.length-1;d>=0;d--){var f=u[d];s(e,t,f.func,f.options)}return}for(var p=Kt(i),m=0;m<u.length;m++){var h=u[m];if(h.func===r&&qt(h.options,p)){e.removeEventListener(t,r,a.supportsOptions?p:p.capture),u.splice(m,1),u.length===0&&(delete c.events[t],l=!0);break}}}l&&!Object.keys(c.events).length&&n.splice(o,1)}else for(t in c.events)c.events.hasOwnProperty(t)&&s(e,t,`all`)}}function c(e,t){for(var n=Kt(t),i=new Gt(e),a=r[e.type],o=Ce(e)[0],s=o;S.element(s);){for(var c=0;c<a.length;c++){var l=a[c],u=l.selector,d=l.context;if(F(s,u)&&M(d,o)&&M(d,s)){var f=l.listeners;i.currentTarget=s;for(var p=0;p<f.length;p++){var m=f[p];qt(m.options,n)&&m.func(i)}}}s=P(s)}}function l(e){return c(e,!0)}return(t=e.document)==null||t.createElement(`div`).addEventListener(`test`,null,{get capture(){return a.supportsOptions=!0},get passive(){return a.supportsPassive=!0}}),e.events=a,a}},Yt={methodOrder:[`simulationResume`,`mouseOrPen`,`hasPointer`,`idle`],search:function(e){for(var t=0,n=Yt.methodOrder;t<n.length;t++){var r=Yt[n[t]](e);if(r)return r}return null},simulationResume:function(e){var t=e.pointerType,n=e.eventType,r=e.eventTarget,i=e.scope;if(!/down|start/i.test(n))return null;for(var a=0,o=i.interactions.list;a<o.length;a++){var s=o[a],c=r;if(s.simulation&&s.simulation.allowResume&&s.pointerType===t)for(;c;){if(c===s.element)return s;c=P(c)}}return null},mouseOrPen:function(e){var t,n=e.pointerId,r=e.pointerType,i=e.eventType,a=e.scope;if(r!==`mouse`&&r!==`pen`)return null;for(var o=0,s=a.interactions.list;o<s.length;o++){var c=s[o];if(c.pointerType===r){if(c.simulation&&!Xt(c,n))continue;if(c.interacting())return c;t||=c}}if(t)return t;for(var l=0,u=a.interactions.list;l<u.length;l++){var d=u[l];if(!(d.pointerType!==r||/down/i.test(i)&&d.simulation))return d}return null},hasPointer:function(e){for(var t=e.pointerId,n=0,r=e.scope.interactions.list;n<r.length;n++){var i=r[n];if(Xt(i,t))return i}return null},idle:function(e){for(var t=e.pointerType,n=0,r=e.scope.interactions.list;n<r.length;n++){var i=r[n];if(i.pointers.length===1){var a=i.interactable;if(a&&(!a.options.gesture||!a.options.gesture.enabled))continue}else if(i.pointers.length>=2)continue;if(!i.interacting()&&t===i.pointerType)return i}return null}};function Xt(e,t){return e.pointers.some((function(e){return e.id===t}))}var Zt=Yt,Qt=[`pointerDown`,`pointerMove`,`pointerUp`,`updatePointer`,`removePointer`,`windowBlur`];function $t(e,t){return function(n){var r=t.interactions.list,i=G(n),a=Ce(n),o=a[0],s=a[1],c=[];if(/^touch/.test(n.type)){t.prevTouchTime=t.now();for(var l=0,u=n.changedTouches;l<u.length;l++){var d=u[l],f={pointer:d,pointerId:ge(d),pointerType:i,eventType:n.type,eventTarget:o,curEventTarget:s,scope:t},p=en(f);c.push([f.pointer,f.eventTarget,f.curEventTarget,p])}}else{var m=!1;if(!j.supportsPointerEvent&&/mouse/.test(n.type)){for(var h=0;h<r.length&&!m;h++)m=r[h].pointerType!==`mouse`&&r[h].pointerIsDown;m=m||t.now()-t.prevTouchTime<500||n.timeStamp===0}if(!m){var g={pointer:n,pointerId:ge(n),pointerType:i,eventType:n.type,curEventTarget:s,eventTarget:o,scope:t},_=en(g);c.push([g.pointer,g.eventTarget,g.curEventTarget,_])}}for(var v=0;v<c.length;v++){var y=c[v],b=y[0],x=y[1],S=y[2];y[3][e](b,n,x,S)}}}function en(e){var t=e.pointerType,n=e.scope,r={interaction:Zt.search(e),searchDetails:e};return n.fire(`interactions:find`,r),r.interaction||n.interactions.new({pointerType:t})}function tn(e,t){var n=e.doc,r=e.scope,i=e.options,a=r.interactions.docEvents,o=r.events,s=o[t];for(var c in r.browser.isIOS&&!i.events&&(i.events={passive:!1}),o.delegatedEvents)s(n,c,o.delegateListener),s(n,c,o.delegateUseCapture,!0);for(var l=i&&i.events,u=0;u<a.length;u++){var d=a[u];s(n,d.type,d.listener,l)}}var nn={id:`core/interactions`,install:function(e){for(var t={},n=0;n<Qt.length;n++){var r=Qt[n];t[r]=$t(r,e)}var a,s=j.pEventTypes;function l(){for(var t=0,n=e.interactions.list;t<n.length;t++){var r=n[t];if(r.pointerIsDown&&r.pointerType===`touch`&&!r._interacting)for(var i=function(){var t=o[a];e.documents.some((function(e){return M(e.doc,t.downTarget)}))||r.removePointer(t.pointer,t.event)},a=0,o=r.pointers;a<o.length;a++)i()}}(a=k.PointerEvent?[{type:s.down,listener:l},{type:s.down,listener:t.pointerDown},{type:s.move,listener:t.pointerMove},{type:s.up,listener:t.pointerUp},{type:s.cancel,listener:t.pointerUp}]:[{type:`mousedown`,listener:t.pointerDown},{type:`mousemove`,listener:t.pointerMove},{type:`mouseup`,listener:t.pointerUp},{type:`touchstart`,listener:l},{type:`touchstart`,listener:t.pointerDown},{type:`touchmove`,listener:t.pointerMove},{type:`touchend`,listener:t.pointerUp},{type:`touchcancel`,listener:t.pointerUp}]).push({type:`blur`,listener:function(t){for(var n=0,r=e.interactions.list;n<r.length;n++)r[n].documentBlur(t)}}),e.prevTouchTime=0,e.Interaction=function(t){c(r,t);var n=f(r);function r(){return i(this,r),n.apply(this,arguments)}return o(r,[{key:`pointerMoveTolerance`,get:function(){return e.interactions.pointerMoveTolerance},set:function(t){e.interactions.pointerMoveTolerance=t}},{key:`_now`,value:function(){return e.now()}}]),r}(jt),e.interactions={list:[],new:function(t){t.scopeFire=function(t,n){return e.fire(t,n)};var n=new e.Interaction(t);return e.interactions.list.push(n),n},listeners:t,docEvents:a,pointerMoveTolerance:1},e.usePlugin(_t)},listeners:{"scope:add-document":function(e){return tn(e,`add`)},"scope:remove-document":function(e){return tn(e,`remove`)},"interactable:unset":function(e,t){for(var n=e.interactable,r=t.interactions.list.length-1;r>=0;r--){var i=t.interactions.list[r];i.interactable===n&&(i.stop(),t.fire(`interactions:destroy`,{interaction:i}),i.destroy(),t.interactions.list.length>2&&t.interactions.list.splice(r,1))}}},onDocSignal:tn,doOnInteractions:$t,methodNames:Qt},rn=function(e){return e[e.On=0]=`On`,e[e.Off=1]=`Off`,e}(rn||{}),an=function(){function e(t,n,r,a){i(this,e),this.target=void 0,this.options=void 0,this._actions=void 0,this.events=new Wt,this._context=void 0,this._win=void 0,this._doc=void 0,this._scopeEvents=void 0,this._actions=n.actions,this.target=t,this._context=n.context||r,this._win=y(oe(t)?this._context:t),this._doc=this._win.document,this._scopeEvents=a,this.set(n)}return o(e,[{key:`_defaults`,get:function(){return{base:{},perAction:{},actions:{}}}},{key:`setOnEvents`,value:function(e,t){return S.func(t.onstart)&&this.on(`${e}start`,t.onstart),S.func(t.onmove)&&this.on(`${e}move`,t.onmove),S.func(t.onend)&&this.on(`${e}end`,t.onend),S.func(t.oninertiastart)&&this.on(`${e}inertiastart`,t.oninertiastart),this}},{key:`updatePerActionListeners`,value:function(e,t,n){var r=this,i=this._actions.map[e]?.filterEventType,a=function(e){return(i==null||i(e))&&vt(e,r._actions)};(S.array(t)||S.object(t))&&this._onOff(rn.Off,e,t,void 0,a),(S.array(n)||S.object(n))&&this._onOff(rn.On,e,n,void 0,a)}},{key:`setPerAction`,value:function(e,t){var n=this._defaults;for(var r in t){var i=r,a=this.options[e],o=t[i];i===`listeners`&&this.updatePerActionListeners(e,a.listeners,o),S.array(o)?a[i]=Ee(o):S.plainObject(o)?(a[i]=L(a[i]||{},yt(o)),S.object(n.perAction[i])&&`enabled`in n.perAction[i]&&(a[i].enabled=!1!==o.enabled)):S.bool(o)&&S.object(n.perAction[i])?a[i].enabled=o:a[i]=o}}},{key:`getRect`,value:function(e){return e||=S.element(this.target)?this.target:null,S.string(this.target)&&(e||=this._context.querySelector(this.target)),ie(e)}},{key:`rectChecker`,value:function(e){var t=this;return S.func(e)?(this.getRect=function(n){var r=L({},e.apply(t,n));return`width`in r||(r.width=r.right-r.left,r.height=r.bottom-r.top),r},this):e===null?(delete this.getRect,this):this.getRect}},{key:`_backCompatOption`,value:function(e,t){if(oe(t)||S.object(t)){for(var n in this.options[e]=t,this._actions.map)this.options[n][e]=t;return this}return this.options[e]}},{key:`origin`,value:function(e){return this._backCompatOption(`origin`,e)}},{key:`deltaSource`,value:function(e){return e===`page`||e===`client`?(this.options.deltaSource=e,this):this.options.deltaSource}},{key:`getAllElements`,value:function(){var e=this.target;return S.string(e)?Array.from(this._context.querySelectorAll(e)):S.func(e)&&e.getAllElements?e.getAllElements():S.element(e)?[e]:[]}},{key:`context`,value:function(){return this._context}},{key:`inContext`,value:function(e){return this._context===e.ownerDocument||M(this._context,e)}},{key:`testIgnoreAllow`,value:function(e,t,n){return!this.testIgnore(e.ignoreFrom,t,n)&&this.testAllow(e.allowFrom,t,n)}},{key:`testAllow`,value:function(e,t,n){return!e||!!S.element(n)&&(S.string(e)?te(n,e,t):!!S.element(e)&&M(e,n))}},{key:`testIgnore`,value:function(e,t,n){return!(!e||!S.element(n))&&(S.string(e)?te(n,e,t):!!S.element(e)&&M(e,n))}},{key:`fire`,value:function(e){return this.events.fire(e),this}},{key:`_onOff`,value:function(e,t,n,r,i){S.object(t)&&!S.array(t)&&(r=n,n=null);var a=V(t,n,i);for(var o in a){o===`wheel`&&(o=j.wheelEvent);for(var s=0,c=a[o];s<c.length;s++){var l=c[s];vt(o,this._actions)?this.events[e===rn.On?`on`:`off`](o,l):S.string(this.target)?this._scopeEvents[e===rn.On?`addDelegate`:`removeDelegate`](this.target,this._context,o,l,r):this._scopeEvents[e===rn.On?`add`:`remove`](this.target,o,l,r)}}return this}},{key:`on`,value:function(e,t,n){return this._onOff(rn.On,e,t,n)}},{key:`off`,value:function(e,t,n){return this._onOff(rn.Off,e,t,n)}},{key:`set`,value:function(e){var t=this._defaults;for(var n in S.object(e)||(e={}),this.options=yt(t.base),this._actions.methodDict){var r=n,i=this._actions.methodDict[r];this.options[r]={},this.setPerAction(r,L(L({},t.perAction),t.actions[r])),this[i](e[r])}for(var a in e)a===`getRect`?this.rectChecker(e.getRect):S.func(this[a])&&this[a](e[a]);return this}},{key:`unset`,value:function(){if(S.string(this.target))for(var e in this._scopeEvents.delegatedEvents)for(var t=this._scopeEvents.delegatedEvents[e],n=t.length-1;n>=0;n--){var r=t[n],i=r.selector,a=r.context,o=r.listeners;i===this.target&&a===this._context&&t.splice(n,1);for(var s=o.length-1;s>=0;s--)this._scopeEvents.removeDelegate(this.target,this._context,e,o[s][0],o[s][1])}else this._scopeEvents.remove(this.target,`all`)}}]),e}(),on=function(){function e(t){var n=this;i(this,e),this.list=[],this.selectorMap={},this.scope=void 0,this.scope=t,t.addListeners({"interactable:unset":function(e){var t=e.interactable,r=t.target,i=S.string(r)?n.selectorMap[r]:r[n.scope.id],a=De(i,(function(e){return e===t}));i.splice(a,1)}})}return o(e,[{key:`new`,value:function(e,t){t=L(t||{},{actions:this.scope.actions});var n=new this.scope.Interactable(e,t,this.scope.document,this.scope.events);return this.scope.addDocument(n._doc),this.list.push(n),S.string(e)?(this.selectorMap[e]||(this.selectorMap[e]=[]),this.selectorMap[e].push(n)):(n.target[this.scope.id]||Object.defineProperty(e,this.scope.id,{value:[],configurable:!0}),e[this.scope.id].push(n)),this.scope.fire(`interactable:new`,{target:e,options:t,interactable:n,win:this.scope._win}),n}},{key:`getExisting`,value:function(e,t){var n=t&&t.context||this.scope.document,r=S.string(e),i=r?this.selectorMap[e]:e[this.scope.id];if(i)return Oe(i,(function(t){return t._context===n&&(r||t.inContext(e))}))}},{key:`forEachMatch`,value:function(e,t){for(var n=0,r=this.list;n<r.length;n++){var i=r[n],a=void 0;if((S.string(i.target)?S.element(e)&&F(e,i.target):e===i.target)&&i.inContext(e)&&(a=t(i)),a!==void 0)return a}}}]),e}(),sn=function(){function e(){var t=this;i(this,e),this.id=`__interact_scope_${Math.floor(100*Math.random())}`,this.isInitialized=!1,this.listenerMaps=[],this.browser=j,this.defaults=yt(Tt),this.Eventable=Wt,this.actions={map:{},phases:{start:!0,move:!0,end:!0},methodDict:{},phaselessTypes:{}},this.interactStatic=function(e){var t=function t(n,r){var i=e.interactables.getExisting(n,r);return i||((i=e.interactables.new(n,r)).events.global=t.globalEvents),i};return t.getPointerAverage=ye,t.getTouchBBox=be,t.getTouchDistance=xe,t.getTouchAngle=Se,t.getElementRect=ie,t.getElementClientRect=re,t.matchesSelector=F,t.closest=N,t.globalEvents={},t.version=`1.10.27`,t.scope=e,t.use=function(e,t){return this.scope.usePlugin(e,t),this},t.isSet=function(e,t){return!!this.scope.interactables.get(e,t&&t.context)},t.on=Qe((function(e,t,n){if(S.string(e)&&e.search(` `)!==-1&&(e=e.trim().split(/ +/)),S.array(e)){for(var r=0,i=e;r<i.length;r++){var a=i[r];this.on(a,t,n)}return this}if(S.object(e)){for(var o in e)this.on(o,e[o],t);return this}return vt(e,this.scope.actions)?this.globalEvents[e]?this.globalEvents[e].push(t):this.globalEvents[e]=[t]:this.scope.events.add(this.scope.document,e,t,{options:n}),this}),`The interact.on() method is being deprecated`),t.off=Qe((function(e,t,n){if(S.string(e)&&e.search(` `)!==-1&&(e=e.trim().split(/ +/)),S.array(e)){for(var r=0,i=e;r<i.length;r++){var a=i[r];this.off(a,t,n)}return this}if(S.object(e)){for(var o in e)this.off(o,e[o],t);return this}var s;return vt(e,this.scope.actions)?e in this.globalEvents&&(s=this.globalEvents[e].indexOf(t))!==-1&&this.globalEvents[e].splice(s,1):this.scope.events.remove(this.scope.document,e,t,n),this}),`The interact.off() method is being deprecated`),t.debug=function(){return this.scope},t.supportsTouch=function(){return j.supportsTouch},t.supportsPointerEvent=function(){return j.supportsPointerEvent},t.stop=function(){for(var e=0,t=this.scope.interactions.list;e<t.length;e++)t[e].stop();return this},t.pointerMoveTolerance=function(e){return S.number(e)?(this.scope.interactions.pointerMoveTolerance=e,this):this.scope.interactions.pointerMoveTolerance},t.addDocument=function(e,t){this.scope.addDocument(e,t)},t.removeDocument=function(e){this.scope.removeDocument(e)},t}(this),this.InteractEvent=Et,this.Interactable=void 0,this.interactables=new on(this),this._win=void 0,this.document=void 0,this.window=void 0,this.documents=[],this._plugins={list:[],map:{}},this.onWindowUnload=function(e){return t.removeDocument(e.target)};var n=this;this.Interactable=function(e){c(r,e);var t=f(r);function r(){return i(this,r),t.apply(this,arguments)}return o(r,[{key:`_defaults`,get:function(){return n.defaults}},{key:`set`,value:function(e){return p(l(r.prototype),`set`,this).call(this,e),n.fire(`interactable:set`,{options:e,interactable:this}),this}},{key:`unset`,value:function(){p(l(r.prototype),`unset`,this).call(this);var e=n.interactables.list.indexOf(this);e<0||(n.interactables.list.splice(e,1),n.fire(`interactable:unset`,{interactable:this}))}}]),r}(an)}return o(e,[{key:`addListeners`,value:function(e,t){this.listenerMaps.push({id:t,map:e})}},{key:`fire`,value:function(e,t){for(var n=0,r=this.listenerMaps;n<r.length;n++){var i=r[n].map[e];if(i&&!1===i(t,this,e))return!1}}},{key:`init`,value:function(e){return this.isInitialized?this:function(e,t){return e.isInitialized=!0,S.window(t)&&v(t),k.init(t),j.init(t),Je.init(t),e.window=t,e.document=t.document,e.usePlugin(nn),e.usePlugin(Jt),e}(this,e)}},{key:`pluginIsInstalled`,value:function(e){var t=e.id;return t?!!this._plugins.map[t]:this._plugins.list.indexOf(e)!==-1}},{key:`usePlugin`,value:function(e,t){if(!this.isInitialized||this.pluginIsInstalled(e))return this;if(e.id&&(this._plugins.map[e.id]=e),this._plugins.list.push(e),e.install&&e.install(this,t),e.listeners&&e.before){for(var n=0,r=this.listenerMaps.length,i=e.before.reduce((function(e,t){return e[t]=!0,e[cn(t)]=!0,e}),{});n<r;n++){var a=this.listenerMaps[n].id;if(a&&(i[a]||i[cn(a)]))break}this.listenerMaps.splice(n,0,{id:e.id,map:e.listeners})}else e.listeners&&this.listenerMaps.push({id:e.id,map:e.listeners});return this}},{key:`addDocument`,value:function(e,t){if(this.getDocIndex(e)!==-1)return!1;var n=y(e);t=t?L({},t):{},this.documents.push({doc:e,options:t}),this.events.documents.push(e),e!==this.document&&this.events.add(n,`unload`,this.onWindowUnload),this.fire(`scope:add-document`,{doc:e,window:n,scope:this,options:t})}},{key:`removeDocument`,value:function(e){var t=this.getDocIndex(e),n=y(e),r=this.documents[t].options;this.events.remove(n,`unload`,this.onWindowUnload),this.documents.splice(t,1),this.events.documents.splice(t,1),this.fire(`scope:remove-document`,{doc:e,window:n,scope:this,options:r})}},{key:`getDocIndex`,value:function(e){for(var t=0;t<this.documents.length;t++)if(this.documents[t].doc===e)return t;return-1}},{key:`getDocOptions`,value:function(e){var t=this.getDocIndex(e);return t===-1?null:this.documents[t].options}},{key:`now`,value:function(){return(this.window.Date||Date).now()}}]),e}();function cn(e){return e&&e.replace(/\/.*$/,``)}var ln=new sn,un=ln.interactStatic,dn=typeof globalThis<`u`?globalThis:window;ln.init(dn);var fn=Object.freeze({__proto__:null,edgeTarget:function(){},elements:function(){},grid:function(e){var t=[[`x`,`y`],[`left`,`top`],[`right`,`bottom`],[`width`,`height`]].filter((function(t){var n=t[0],r=t[1];return n in e||r in e})),n=function(n,r){for(var i=e.range,a=e.limits,o=a===void 0?{left:-1/0,right:1/0,top:-1/0,bottom:1/0}:a,s=e.offset,c=s===void 0?{x:0,y:0}:s,l={range:i,grid:e,x:null,y:null},u=0;u<t.length;u++){var d=t[u],f=d[0],p=d[1],m=Math.round((n-c.x)/e[f]),h=Math.round((r-c.y)/e[p]);l[f]=Math.max(o.left,Math.min(o.right,m*e[f]+c.x)),l[p]=Math.max(o.top,Math.min(o.bottom,h*e[p]+c.y))}return l};return n.grid=e,n.coordFields=t,n}}),pn={id:`snappers`,install:function(e){var t=e.interactStatic;t.snappers=L(t.snappers||{},fn),t.createSnapGrid=t.snappers.grid}},mn={start:function(e){var t=e.state,r=e.rect,i=e.edges,a=e.pageCoords,o=t.options,s=o.ratio,c=o.enabled,l=t.options,u=l.equalDelta,d=l.modifiers;s===`preserve`&&(s=r.width/r.height),t.startCoords=L({},a),t.startRect=L({},r),t.ratio=s,t.equalDelta=u;var f=t.linkedEdges={top:i.top||i.left&&!i.bottom,left:i.left||i.top&&!i.right,bottom:i.bottom||i.right&&!i.top,right:i.right||i.bottom&&!i.left};if(t.xIsPrimaryAxis=!(!i.left&&!i.right),t.equalDelta){var p=(f.left?1:-1)*(f.top?1:-1);t.edgeSign={x:p,y:p}}else t.edgeSign={x:f.left?-1:1,y:f.top?-1:1};if(!1!==c&&L(i,f),d!=null&&d.length){var m=new bt(e.interaction);m.copyFrom(e.interaction.modification),m.prepareStates(d),t.subModification=m,m.startAll(n({},e))}},set:function(e){var t=e.state,r=e.rect,i=e.coords,a=t.linkedEdges,o=L({},i),s=t.equalDelta?hn:gn;if(L(e.edges,a),s(t,t.xIsPrimaryAxis,i,r),!t.subModification)return null;var c=L({},r);z(a,c,{x:i.x-o.x,y:i.y-o.y});var l=t.subModification.setAll(n(n({},e),{},{rect:c,edges:a,pageCoords:i,prevCoords:i,prevRect:c})),u=l.delta;return l.changed&&(s(t,Math.abs(u.x)>Math.abs(u.y),l.coords,l.rect),L(i,l.coords)),l.eventProps},defaults:{ratio:`preserve`,equalDelta:!1,modifiers:[],enabled:!1}};function hn(e,t,n){var r=e.startCoords,i=e.edgeSign;t?n.y=r.y+(n.x-r.x)*i.y:n.x=r.x+(n.y-r.y)*i.x}function gn(e,t,n,r){var i=e.startRect,a=e.startCoords,o=e.ratio,s=e.edgeSign;if(t){var c=r.width/o;n.y=a.y+(c-i.height)*s.y}else{var l=r.height*o;n.x=a.x+(l-i.width)*s.x}}var _n=St(mn,`aspectRatio`),vn=function(){};vn._defaults={};var yn=vn;function bn(e,t,n){return S.func(e)?ce(e,t.interactable,t.element,[n.x,n.y,t]):ce(e,t.interactable,t.element)}var xn={start:function(e){var t=e.rect,n=e.startOffset,r=e.state,i=e.interaction,a=e.pageCoords,o=r.options,s=o.elementRect,c=L({left:0,top:0,right:0,bottom:0},o.offset||{});if(t&&s){var l=bn(o.restriction,i,a);if(l){var u=l.right-l.left-t.width,d=l.bottom-l.top-t.height;u<0&&(c.left+=u,c.right+=u),d<0&&(c.top+=d,c.bottom+=d)}c.left+=n.left-t.width*s.left,c.top+=n.top-t.height*s.top,c.right+=n.right-t.width*(1-s.right),c.bottom+=n.bottom-t.height*(1-s.bottom)}r.offset=c},set:function(e){var t=e.coords,n=e.interaction,r=e.state,i=r.options,a=r.offset,o=bn(i.restriction,n,t);if(o){var s=function(e){return!e||`left`in e&&`top`in e||((e=L({},e)).left=e.x||0,e.top=e.y||0,e.right=e.right||e.left+e.width,e.bottom=e.bottom||e.top+e.height),e}(o);t.x=Math.max(Math.min(s.right-a.right,t.x),s.left+a.left),t.y=Math.max(Math.min(s.bottom-a.bottom,t.y),s.top+a.top)}},defaults:{restriction:null,elementRect:null,offset:null,endOnly:!1,enabled:!1}},Sn=St(xn,`restrict`),Cn={top:1/0,left:1/0,bottom:-1/0,right:-1/0},wn={top:-1/0,left:-1/0,bottom:1/0,right:1/0};function Tn(e,t){for(var n=0,r=[`top`,`left`,`bottom`,`right`];n<r.length;n++){var i=r[n];i in e||(e[i]=t[i])}return e}var En={noInner:Cn,noOuter:wn,start:function(e){var t,n=e.interaction,r=e.startOffset,i=e.state,a=i.options;a&&(t=R(bn(a.offset,n,n.coords.start.page))),t||={x:0,y:0},i.offset={top:t.y+r.top,left:t.x+r.left,bottom:t.y-r.bottom,right:t.x-r.right}},set:function(e){var t=e.coords,n=e.edges,r=e.interaction,i=e.state,a=i.offset,o=i.options;if(n){var s=L({},t),c=bn(o.inner,r,s)||{},l=bn(o.outer,r,s)||{};Tn(c,Cn),Tn(l,wn),n.top?t.y=Math.min(Math.max(l.top+a.top,s.y),c.top+a.top):n.bottom&&(t.y=Math.max(Math.min(l.bottom+a.bottom,s.y),c.bottom+a.bottom)),n.left?t.x=Math.min(Math.max(l.left+a.left,s.x),c.left+a.left):n.right&&(t.x=Math.max(Math.min(l.right+a.right,s.x),c.right+a.right))}},defaults:{inner:null,outer:null,offset:null,endOnly:!1,enabled:!1}},Dn=St(En,`restrictEdges`),On=L({get elementRect(){return{top:0,left:0,bottom:1,right:1}},set elementRect(e){}},xn.defaults),kn=St({start:xn.start,set:xn.set,defaults:On},`restrictRect`),An={width:-1/0,height:-1/0},jn={width:1/0,height:1/0},Mn=St({start:function(e){return En.start(e)},set:function(e){var t=e.interaction,n=e.state,r=e.rect,i=e.edges,a=n.options;if(i){var o=le(bn(a.min,t,e.coords))||An,s=le(bn(a.max,t,e.coords))||jn;n.options={endOnly:a.endOnly,inner:L({},En.noInner),outer:L({},En.noOuter)},i.top?(n.options.inner.top=r.bottom-o.height,n.options.outer.top=r.bottom-s.height):i.bottom&&(n.options.inner.bottom=r.top+o.height,n.options.outer.bottom=r.top+s.height),i.left?(n.options.inner.left=r.right-o.width,n.options.outer.left=r.right-s.width):i.right&&(n.options.inner.right=r.left+o.width,n.options.outer.right=r.left+s.width),En.set(e),n.options=a}},defaults:{min:null,max:null,endOnly:!1,enabled:!1}},`restrictSize`),Nn={start:function(e){var t,n=e.interaction,r=e.interactable,i=e.element,a=e.rect,o=e.state,s=e.startOffset,c=o.options,l=c.offsetWithOrigin?function(e){var t=e.interaction.element;return R(ce(e.state.options.origin,null,null,[t]))||B(e.interactable,t,e.interaction.prepared.name)}(e):{x:0,y:0};if(c.offset===`startCoords`)t={x:n.coords.start.page.x,y:n.coords.start.page.y};else{var u=ce(c.offset,r,i,[n]);(t=R(u)||{x:0,y:0}).x+=l.x,t.y+=l.y}var d=c.relativePoints;o.offsets=a&&d&&d.length?d.map((function(e,n){return{index:n,relativePoint:e,x:s.left-a.width*e.x+t.x,y:s.top-a.height*e.y+t.y}})):[{index:0,relativePoint:null,x:t.x,y:t.y}]},set:function(e){var t=e.interaction,n=e.coords,r=e.state,i=r.options,a=r.offsets,o=B(t.interactable,t.element,t.prepared.name),s=L({},n),c=[];i.offsetWithOrigin||(s.x-=o.x,s.y-=o.y);for(var l=0,u=a;l<u.length;l++)for(var d=u[l],f=s.x-d.x,p=s.y-d.y,m=0,h=i.targets.length;m<h;m++){var g=i.targets[m],_=void 0;(_=S.func(g)?g(f,p,t._proxy,d,m):g)&&c.push({x:(S.number(_.x)?_.x:f)+d.x,y:(S.number(_.y)?_.y:p)+d.y,range:S.number(_.range)?_.range:i.range,source:g,index:m,offset:d})}for(var v={target:null,inRange:!1,distance:0,range:0,delta:{x:0,y:0}},y=0;y<c.length;y++){var b=c[y],x=b.range,C=b.x-s.x,w=b.y-s.y,T=U(C,w),E=T<=x;x===1/0&&v.inRange&&v.range!==1/0&&(E=!1),v.target&&!(E?v.inRange&&x!==1/0?T/x<v.distance/v.range:x===1/0&&v.range!==1/0||T<v.distance:!v.inRange&&T<v.distance)||(v.target=b,v.distance=T,v.range=x,v.inRange=E,v.delta.x=C,v.delta.y=w)}return v.inRange&&(n.x=v.target.x,n.y=v.target.y),r.closest=v,v},defaults:{range:1/0,targets:null,offset:null,offsetWithOrigin:!0,origin:null,relativePoints:null,endOnly:!1,enabled:!1}},Pn=St(Nn,`snap`),Fn={start:function(e){var t=e.state,n=e.edges,r=t.options;if(!n)return null;e.state={options:{targets:null,relativePoints:[{x:n.left?0:1,y:n.top?0:1}],offset:r.offset||`self`,origin:{x:0,y:0},range:r.range}},t.targetFields=t.targetFields||[[`width`,`height`],[`x`,`y`]],Nn.start(e),t.offsets=e.state.offsets,e.state=t},set:function(e){var t=e.interaction,n=e.state,r=e.coords,i=n.options,a=n.offsets,o={x:r.x-a[0].x,y:r.y-a[0].y};n.options=L({},i),n.options.targets=[];for(var s=0,c=i.targets||[];s<c.length;s++){var l=c[s],u=void 0;if(u=S.func(l)?l(o.x,o.y,t):l){for(var d=0,f=n.targetFields;d<f.length;d++){var p=f[d],m=p[0],h=p[1];if(m in u||h in u){u.x=u[m],u.y=u[h];break}}n.options.targets.push(u)}}var g=Nn.set(e);return n.options=i,g},defaults:{range:1/0,targets:null,offset:null,endOnly:!1,enabled:!1}},In=St(Fn,`snapSize`),Ln={aspectRatio:_n,restrictEdges:Dn,restrict:Sn,restrictRect:kn,restrictSize:Mn,snapEdges:St({start:function(e){var t=e.edges;return t?(e.state.targetFields=e.state.targetFields||[[t.left?`left`:`right`,t.top?`top`:`bottom`]],Fn.start(e)):null},set:Fn.set,defaults:L(yt(Fn.defaults),{targets:void 0,range:void 0,offset:{x:0,y:0}})},`snapEdges`),snap:Pn,snapSize:In,spring:yn,avoid:yn,transform:yn,rubberband:yn},Rn={id:`modifiers`,install:function(e){var t=e.interactStatic;for(var n in e.usePlugin(wt),e.usePlugin(pn),t.modifiers=Ln,Ln){var r=Ln[n],i=r._defaults;i._methods=r._methods,e.defaults.perAction[n]=i}}},zn=function(e){c(n,e);var t=f(n);function n(e,r,a,o,s,c){var l;if(i(this,n),ue(d(l=t.call(this,s)),a),a!==r&&ue(d(l),r),l.timeStamp=c,l.originalEvent=a,l.type=e,l.pointerId=ge(r),l.pointerType=G(r),l.target=o,l.currentTarget=null,e===`tap`){var u=s.getPointerIndex(r);l.dt=l.timeStamp-s.pointers[u].downTime;var f=l.timeStamp-s.tapTime;l.double=!!s.prevTap&&s.prevTap.type!==`doubletap`&&s.prevTap.target===l.target&&f<500}else e===`doubletap`&&(l.dt=r.timeStamp-s.tapTime,l.double=!0);return l}return o(n,[{key:`_subtractOrigin`,value:function(e){var t=e.x,n=e.y;return this.pageX-=t,this.pageY-=n,this.clientX-=t,this.clientY-=n,this}},{key:`_addOrigin`,value:function(e){var t=e.x,n=e.y;return this.pageX+=t,this.pageY+=n,this.clientX+=t,this.clientY+=n,this}},{key:`preventDefault`,value:function(){this.originalEvent.preventDefault()}}]),n}(we),Bn={id:`pointer-events/base`,before:[`inertia`,`modifiers`,`auto-start`,`actions`],install:function(e){e.pointerEvents=Bn,e.defaults.actions.pointerEvents=Bn.defaults,L(e.actions.phaselessTypes,Bn.types)},listeners:{"interactions:new":function(e){var t=e.interaction;t.prevTap=null,t.tapTime=0},"interactions:update-pointer":function(e){var t=e.down,n=e.pointerInfo;!t&&n.hold||(n.hold={duration:1/0,timeout:null})},"interactions:move":function(e,t){var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget;e.duplicate||n.pointerIsDown&&!n.pointerWasMoved||(n.pointerIsDown&&Un(e),Vn({interaction:n,pointer:r,event:i,eventTarget:a,type:`move`},t))},"interactions:down":function(e,t){(function(e,t){for(var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget,o=e.pointerIndex,s=n.pointers[o].hold,c=ae(a),l={interaction:n,pointer:r,event:i,eventTarget:a,type:`hold`,targets:[],path:c,node:null},u=0;u<c.length;u++)l.node=c[u],t.fire(`pointerEvents:collect-targets`,l);if(l.targets.length){for(var d=1/0,f=0,p=l.targets;f<p.length;f++){var m=p[f].eventable.options.holdDuration;m<d&&(d=m)}s.duration=d,s.timeout=setTimeout((function(){Vn({interaction:n,eventTarget:a,pointer:r,event:i,type:`hold`},t)}),d)}})(e,t),Vn(e,t)},"interactions:up":function(e,t){Un(e),Vn(e,t),function(e,t){var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget;n.pointerWasMoved||Vn({interaction:n,eventTarget:a,pointer:r,event:i,type:`tap`},t)}(e,t)},"interactions:cancel":function(e,t){Un(e),Vn(e,t)}},PointerEvent:zn,fire:Vn,collectEventTargets:Hn,defaults:{holdDuration:600,ignoreFrom:null,allowFrom:null,origin:{x:0,y:0}},types:{down:!0,move:!0,up:!0,cancel:!0,tap:!0,doubletap:!0,hold:!0}};function Vn(e,t){var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget,o=e.type,s=e.targets,c=s===void 0?Hn(e,t):s,l=new zn(o,r,i,a,n,t.now());t.fire(`pointerEvents:new`,{pointerEvent:l});for(var u={interaction:n,pointer:r,event:i,eventTarget:a,targets:c,type:o,pointerEvent:l},d=0;d<c.length;d++){var f=c[d];for(var p in f.props||{})l[p]=f.props[p];var m=B(f.eventable,f.node);if(l._subtractOrigin(m),l.eventable=f.eventable,l.currentTarget=f.node,f.eventable.fire(l),l._addOrigin(m),l.immediatePropagationStopped||l.propagationStopped&&d+1<c.length&&c[d+1].node!==l.currentTarget)break}if(t.fire(`pointerEvents:fired`,u),o===`tap`){var h=l.double?Vn({interaction:n,pointer:r,event:i,eventTarget:a,type:`doubletap`},t):l;n.prevTap=h,n.tapTime=h.timeStamp}return l}function Hn(e,t){var n=e.interaction,r=e.pointer,i=e.event,a=e.eventTarget,o=e.type,s=n.getPointerIndex(r),c=n.pointers[s];if(o===`tap`&&(n.pointerWasMoved||!c||c.downTarget!==a))return[];for(var l=ae(a),u={interaction:n,pointer:r,event:i,eventTarget:a,type:o,path:l,targets:[],node:null},d=0;d<l.length;d++)u.node=l[d],t.fire(`pointerEvents:collect-targets`,u);return o===`hold`&&(u.targets=u.targets.filter((function(e){var t,r;return e.eventable.options.holdDuration===((t=n.pointers[s])==null||(r=t.hold)==null?void 0:r.duration)}))),u.targets}function Un(e){var t=e.interaction,n=e.pointerIndex,r=t.pointers[n].hold;r&&r.timeout&&(clearTimeout(r.timeout),r.timeout=null)}var Wn=Object.freeze({__proto__:null,default:Bn});function Gn(e){var t=e.interaction;t.holdIntervalHandle&&=(clearInterval(t.holdIntervalHandle),null)}var Kn={id:`pointer-events/holdRepeat`,install:function(e){e.usePlugin(Bn);var t=e.pointerEvents;t.defaults.holdRepeatInterval=0,t.types.holdrepeat=e.actions.phaselessTypes.holdrepeat=!0},listeners:[`move`,`up`,`cancel`,`endall`].reduce((function(e,t){return e[`pointerEvents:${t}`]=Gn,e}),{"pointerEvents:new":function(e){var t=e.pointerEvent;t.type===`hold`&&(t.count=(t.count||0)+1)},"pointerEvents:fired":function(e,t){var n=e.interaction,r=e.pointerEvent,i=e.eventTarget,a=e.targets;if(r.type===`hold`&&a.length){var o=a[0].eventable.options.holdRepeatInterval;o<=0||(n.holdIntervalHandle=setTimeout((function(){t.pointerEvents.fire({interaction:n,eventTarget:i,type:`hold`,pointer:r,event:r},t)}),o))}}})},qn={id:`pointer-events/interactableTargets`,install:function(e){var t=e.Interactable;t.prototype.pointerEvents=function(e){return L(this.events.options,e),this};var n=t.prototype._backCompatOption;t.prototype._backCompatOption=function(e,t){var r=n.call(this,e,t);return r===this&&(this.events.options[e]=t),r}},listeners:{"pointerEvents:collect-targets":function(e,t){var n=e.targets,r=e.node,i=e.type,a=e.eventTarget;t.interactables.forEachMatch(r,(function(e){var t=e.events,o=t.options;t.types[i]&&t.types[i].length&&e.testIgnoreAllow(o,r,a)&&n.push({node:r,eventable:t,props:{interactable:e}})}))},"interactable:new":function(e){var t=e.interactable;t.events.getRect=function(e){return t.getRect(e)}},"interactable:set":function(e,t){var n=e.interactable,r=e.options;L(n.events.options,t.pointerEvents.defaults),L(n.events.options,r.pointerEvents||{})}}};if(un.use(_t),un.use(It),un.use({id:`pointer-events`,install:function(e){e.usePlugin(Wn),e.usePlugin(Kn),e.usePlugin(qn)}}),un.use(Ht),un.use(Rn),un.use(mt),un.use(Ke),un.use(Ze),un.use({id:`reflow`,install:function(e){var t=e.Interactable;e.actions.phases.reflow=!0,t.prototype.reflow=function(t){return function(e,t,n){for(var r=e.getAllElements(),i=n.window.Promise,a=i?[]:null,o=function(){var o=r[s],c=e.getRect(o);if(!c)return 1;var l,u=Oe(n.interactions.list,(function(n){return n.interacting()&&n.interactable===e&&n.element===o&&n.prepared.name===t.name}));if(u)u.move(),a&&(l=u._reflowPromise||new i((function(e){u._reflowResolve=e})));else{var d=le(c);l=function(e,t,n,r,i){var a=e.interactions.new({pointerType:`reflow`}),o={interaction:a,event:i,pointer:i,eventTarget:n,phase:`reflow`};a.interactable=t,a.element=n,a.prevEvent=i,a.updatePointer(i,i,n,!0),fe(a.coords.delta),$e(a.prepared,r),a._doPhase(o);var s=e.window.Promise,c=s?new s((function(e){a._reflowResolve=e})):void 0;return a._reflowPromise=c,a.start(r,t,n),a._interacting?(a.move(o),a.end(i)):(a.stop(),a._reflowResolve()),a.removePointer(i,i),c}(n,e,o,t,function(e){return{coords:e,get page(){return this.coords.page},get client(){return this.coords.client},get timeStamp(){return this.coords.timeStamp},get pageX(){return this.coords.page.x},get pageY(){return this.coords.page.y},get clientX(){return this.coords.client.x},get clientY(){return this.coords.client.y},get pointerId(){return this.coords.pointerId},get target(){return this.coords.target},get type(){return this.coords.type},get pointerType(){return this.coords.pointerType},get buttons(){return this.coords.buttons},preventDefault:function(){}}}({page:{x:d.x,y:d.y},client:{x:d.x,y:d.y},timeStamp:n.now()}))}a&&a.push(l)},s=0;s<r.length&&!o();s++);return a&&i.all(a).then((function(){return e}))}(this,t,e)}},listeners:{"interactions:stop":function(e,t){var n=e.interaction;n.pointerType===`reflow`&&(n._reflowResolve&&n._reflowResolve(),function(e,t){e.splice(e.indexOf(t),1)}(t.interactions.list,n))}}}),un.default=un,(t===void 0?`undefined`:r(t))===`object`&&t)try{t.exports=un}catch{}return un.default=un,un}))})),Ft=o(((e,t)=>{t.exports={}})),It=c(Pt(),1),Lt=c(Ft(),1),Rt=globalThis.ReadableStream,zt=class{constructor(){this.listeners=new Map}on(e,t){this.listeners.has(e)||this.listeners.set(e,new Set),this.listeners.get(e).add(t)}off(e,t){this.listeners.get(e)?.delete(t)}emit(e,t){this.listeners.get(e)?.forEach(e=>e(t))}},Bt=(e,t={})=>{if(typeof e==`object`&&e)return Bt(e.path,e.parameters);let n=e,r=e=>{if(!e||typeof e!=`object`)return e;if(Array.isArray(e))return e.map(r);let t={},n=Object.keys(e).sort();for(let i of n)t[i]=r(e[i]);return t},i={path:n,parameters:r(t)},a={};return Object.keys(i).sort().forEach(e=>a[e]=i[e]),a},Vt=class extends Error{constructor(){super(`VFS is closed`),this.name=`VFSClosedError`}},Ht=class{constructor(){this.results=new Map}async get(e){let t=this.results.get(e);if(!t)return null;let n=t.data;return new Rt({start(e){n.length>0&&e.enqueue(n),e.close()}})}async set(e,t,n){let r,i=n?.size;if(t instanceof Uint8Array)r=t;else if(typeof t==`string`)r=new TextEncoder().encode(t);else{let e=[];if(typeof t?.getReader==`function`){let n=t.getReader();try{for(;;){let{done:t,value:r}=await n.read();if(t)break;e.push(r)}}finally{n.releaseLock()}}else if(t?.[Symbol.asyncIterator])for await(let n of t)e.push(n);else r=t;if(!r){let t=e.reduce((e,t)=>e+t.length,0);if(i!==void 0&&t!==i)throw Error(`Stream aborted or corrupted: Expected ${i} bytes, got ${t}`);r=new Uint8Array(t);let n=0;for(let t of e)r.set(t,n),n+=t.length}}if(i!==void 0&&r.length!==i)throw Error(`Data size mismatch: Expected ${i} bytes, got ${r.length}`);this.results.set(e,{data:r,info:n})}async has(e){return this.results.has(e)}async delete(e){this.results.delete(e)}async close(){this.results.clear()}async*iterateMeta(){for(let[e,t]of this.results.entries())yield{cid:e,info:t.info}}},Ut=class e{constructor(e={}){this.id=e.id||Lt.default.randomUUID(),this.storage=e.storage||new Ht,this.getCID=e.getCID||(async e=>{let t=Bt(e);return Lt.default.createHash(`sha256`).update(JSON.stringify(t)).digest(`hex`)}),this.providers=new Map,this.schemas=new Map,this.activeWait=new Map,this.events=new zt,this.mesh=null,this.closed=!1}registerProvider(e,t,n={}){this.providers.set(e,t),n.schema&&(this.schemas.set(e,n.schema),this.writeData(`sys/schema`,{target:e},n.schema).catch(e=>console.warn(`[VFS ${this.id}] Schema write failed:`,e)))}addSchema(e,t){this.schemas.set(e,t)}validateSelector(e){let t=this.schemas.get(e.path);if(!t)return!0;let n=e.parameters||{};if(t.required){for(let r of t.required)if(n[r]===void 0)throw Error(`Underconstrained selector for ${e.path}: Missing required parameter '${r}'`)}return!0}async read(e,t,n={}){let r=await this._readResult(e,t,n);return r&&r.success?this.storage.get(r.cid):null}async _readResult(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4,followLinks:a=!0,depth:o=0}=n,s=Bt(e,t);if(o>10)throw Error(`Maximum recursion depth exceeded for ${s.path}`);if(this.validateSelector(s),Date.now()>i)return null;let c=JSON.stringify(s);if(this.activeWait.has(c))return this.activeWait.get(c);let l=(async()=>{let e=await this.getCID(s);try{if(await this.storage.has(e))return{success:!0,cid:e,metadata:(await this._getStorageInfo(e))?.tags||{}};this.events.emit(`trace`,{type:`READ_START`,selector:s,stack:r}),this.mesh&&this.mesh.subscribe(s,i,r).catch(()=>{});let t=this.providers.get(s.path);if(!t)for(let[e,n]of this.providers.entries()){if(e.startsWith(`.`)&&s.path.endsWith(e)){t=n;break}if(e.endsWith(`/`)&&s.path.startsWith(e)){t=n;break}}let c=null,l={};if(t)this.events.emit(`trace`,{type:`HANDLER_START`,selector:s,peer:this.id}),c=await t(this,s,{...n,expiresAt:i});else if(this.mesh){this.events.emit(`trace`,{type:`MESH_START`,selector:s,peer:this.id});let e=await this.mesh.read(s.path,s.parameters,{stack:r,expiresAt:i});e&&(c=e.body||e)}if(c){if(a){let t=await this._peekLink(c);if(t.isLink){let i=await this._extractMetadata(t.text),a=await this._readResult(t.linkPath,t.linkParams||s.parameters,{...n,depth:o+1,stack:[...r,this.id]});if(a&&a.success)c=await this.storage.get(a.cid),l={...i,...a.metadata};else return{success:!1,cid:e}}else c=t.stream}try{let t={path:s.path,parameters:s.parameters,tags:l};return await this.write(s.path,s.parameters,c,t),{success:!0,cid:e,metadata:l}}catch{return{success:!1,cid:e}}}return{success:!1,cid:e}}catch(e){return this.events.emit(`trace`,{type:`ERROR`,selector:s,message:e.message,peer:this.id}),{success:!1,cid:``,error:e.message}}finally{this.activeWait.delete(c),this.events.emit(`trace`,{type:`READ_END`,selector:s})}})();return this.activeWait.set(c,l),l}async _getStorageInfo(e){if(typeof this.storage.iterateMeta==`function`){for await(let t of this.storage.iterateMeta())if(t.cid===e)return t.info}return null}async _extractMetadata(e){try{return(typeof e==`string`?JSON.parse(e):e).tags||{}}catch{return{}}}async _peekLink(e){if(e instanceof Uint8Array||typeof e==`string`){let t=typeof e==`string`?e:new TextDecoder().decode(e);try{let n=JSON.parse(t);if(n.geometry&&typeof n.geometry==`string`&&n.geometry.startsWith(`vfs:/`))return{isLink:!0,linkPath:n.geometry.slice(5),linkParams:n.parameters,stream:e,text:t}}catch{}return{isLink:!1,stream:e,text:null}}let t=e.getReader(),{done:n,value:r}=await t.read();if(n||!r)return t.releaseLock(),{isLink:!1,stream:e,text:null};let i=null;try{i=new TextDecoder().decode(r);let e=JSON.parse(i);if(e.geometry&&typeof e.geometry==`string`&&e.geometry.startsWith(`vfs:/`))return t.releaseLock(),{isLink:!0,linkPath:e.geometry.slice(5),linkParams:e.parameters,stream:null,text:i}}catch{}return{isLink:!1,text:i,stream:new Rt({async start(e){e.enqueue(r);try{for(;;){let{done:n,value:r}=await t.read();if(n)break;e.enqueue(r)}}finally{t.releaseLock(),e.close()}}})}}async write(e,t,n,r={}){this._checkClosed();let i=Bt(e,t),a=await this.getCID(i);try{await this.storage.set(a,n,{...r,path:i.path,parameters:i.parameters}),this.events.emit(`state`,{path:i.path,parameters:i.parameters,cid:a,state:`AVAILABLE`})}catch(e){throw await this.storage.delete(a),e}}async writeData(e,t,n){let r;r=n instanceof Uint8Array?n:typeof n==`string`?new TextEncoder().encode(n):new TextEncoder().encode(JSON.stringify(n,null,2));let i=new Rt({start(e){e.enqueue(r),e.close()}});await this.write(e,t,i)}async readData(e,t,n={}){let r=await this.read(e,t,n);if(!r)return null;let i=[],a=r.getReader();try{for(;;){let{done:e,value:t}=await a.read();if(e)break;i.push(t)}}finally{a.releaseLock()}let o=i.reduce((e,t)=>e+t.length,0),s=new Uint8Array(o),c=0;for(let e of i)s.set(e,c),c+=e.length;let l=new TextDecoder().decode(s);try{let e=l.trim();if(e.startsWith(`{`)||e.startsWith(`[`))return JSON.parse(e)}catch{}return s}async close(){this.closed=!0,this.activeWait.clear(),this.mesh&&this.mesh.stop(),await this.storage.close()}_checkClosed(){if(this.closed)throw new Vt}static formatVFSChunk(e,t=new Uint8Array){let n=JSON.stringify(Bt(e)),r=`\n=${t.length} ${n}\n`,i=new TextEncoder().encode(r),a=new Uint8Array(i.length+t.length);return a.set(i),a.set(t,i.length),a}static async*parseVFSBundle(e){if(!e)return;let t=typeof e.getReader==`function`?e.getReader():null,n=new Uint8Array,r=e=>{let t=new Uint8Array(n.length+e.length);t.set(n),t.set(e,n.length),n=t},i=new TextDecoder;try{for(;t;){{let{done:e,value:i}=await t.read();if(i&&r(i),e&&n.length===0)break}let e=0;for(;e<n.length;){if(n[e]===10&&n[e+1]===61){let t=-1;for(let r=e+2;r<n.length;r++)if(n[r]===10){t=r;break}if(t!==-1){let r=i.decode(n.subarray(e+2,t)),a=r.indexOf(` `);if(a!==-1){let i=parseInt(r.slice(0,a),10),o=r.slice(a+1),s;try{s=JSON.parse(o)}catch{s={path:o}}let c=t+1;if(n.length>=c+i){let t=n.subarray(c,c+i);yield{selector:s,data:new Uint8Array(t)},e=c+i;continue}}}}else if(n[e]!==10){e++;continue}break}if(e>0&&(n=n.slice(e)),t&&n.length===0)break}}finally{t&&t.releaseLock()}}async spy(t,n,r={}){let i=Bt(t,n),{stack:a=[],expiresAt:o=Date.now()+3e4}=r,s=[],c=new Set;if(typeof this.storage.iterateMeta==`function`){for await(let{cid:t,info:n}of this.storage.iterateMeta())if(n&&this._matchesSpy(n,i)&&!c.has(t)){c.add(t);let r=await this.storage.get(t);if(r){let t=await this._streamToBytes(r);s.push(e.formatVFSChunk(n,t))}}}if(this.mesh){let e=await this.mesh.spy(i.path,i.parameters,{stack:a,expiresAt:o});if(e){let t=e.getReader();try{for(;;){let{done:e,value:n}=await t.read();if(e)break;s.push(n)}}finally{t.releaseLock()}}}return s.length===0?null:new Rt({start(e){for(let t of s)e.enqueue(t);e.close()}})}async _streamToBytes(e){if(e instanceof Uint8Array)return e;let t=[],n=e.getReader();try{for(;;){let{done:e,value:r}=await n.read();if(e)break;t.push(r)}}finally{n.releaseLock()}let r=t.reduce((e,t)=>e+t.length,0),i=new Uint8Array(r),a=0;for(let e of t)i.set(e,a),a+=e.length;return i}_matchesSpy(e,t){return e.path?t.path.endsWith(`*`)?e.path.startsWith(t.path.slice(0,-1)):e.path===t.path:!1}},Wt=async e=>{let t=Bt(e.path,e.parameters);if(!t.path)throw Error(`Selector must have a path`);let n=new TextEncoder().encode(t.path+JSON.stringify(t.parameters)),r=await crypto.subtle.digest(`SHA-256`,n);return Array.from(new Uint8Array(r)).map(e=>e.toString(16).padStart(2,`0`)).join(``)},Gt=class{constructor(e=`jotcad-vfs`,t=`results`){this.dbName=e,this.storeName=t,this.db=null}async _getDB(){return this.db?this.db:new Promise((e,t)=>{let n=indexedDB.open(this.dbName,1);n.onupgradeneeded=e=>{let t=e.target.result;t.objectStoreNames.contains(this.storeName)||t.createObjectStore(this.storeName)},n.onsuccess=t=>{this.db=t.target.result,e(this.db)},n.onerror=e=>t(e.target.error)})}async get(e){let t=await this._getDB();return new Promise((n,r)=>{let i=t.transaction(this.storeName,`readonly`).objectStore(this.storeName).get(e);i.onsuccess=async()=>{let e=i.result;if(!e)return n(null);n((e.data instanceof Blob?e.data:new Blob([e.data])).stream())},i.onerror=()=>r(i.error)})}async set(e,t,n){let r,i=0,a=n?.size;if(t instanceof Uint8Array||t instanceof Blob)r=t instanceof Blob?t:new Blob([t]),i=r.size;else if(typeof t==`string`)r=new Blob([t]),i=r.size;else if(t&&typeof t.getReader==`function`){let e=t.getReader(),n=[];try{for(;;){let{done:t,value:r}=await e.read();if(t)break;n.push(r),i+=r.length}r=new Blob(n)}finally{e.releaseLock()}}else r=new Blob([t]),i=r.size;if(a!==void 0&&i!==a)throw Error(`Browser storage write aborted: Expected ${a} bytes, got ${i}`);let o=await this._getDB();return new Promise((t,i)=>{let a=o.transaction(this.storeName,`readwrite`).objectStore(this.storeName).put({info:n,data:r},e);a.onsuccess=()=>t(),a.onerror=()=>i(a.error)})}async has(e){let t=await this._getDB();return new Promise((n,r)=>{let i=t.transaction(this.storeName,`readonly`).objectStore(this.storeName).count(e);i.onsuccess=()=>n(i.result>0),i.onerror=()=>r(i.error)})}async delete(e){let t=await this._getDB();return new Promise((n,r)=>{let i=t.transaction(this.storeName,`readwrite`).objectStore(this.storeName).delete(e);i.onsuccess=()=>n(),i.onerror=()=>r(i.error)})}async close(){this.db&&=(this.db.close(),null)}async*iterateMeta(){let e=(await this._getDB()).transaction(this.storeName,`readonly`).objectStore(this.storeName).openCursor(),t=await new Promise((t,n)=>{let r=[];e.onsuccess=e=>{let n=e.target.result;n?(r.push({cid:n.key,info:n.value.info}),n.continue()):t(r)},e.onerror=()=>n(e.error)});for(let e of t)yield e}},Kt=class extends Ut{constructor(e={}){super({getCID:Wt,...e}),this.initialized=!1}async init(){if(!this.initialized&&(this.initialized=!0,this.storage instanceof Gt)){console.log(`[VFS ${this.id}] EPHEMERAL WIPE: Cleaning IndexedDB storage...`);let e=await this.storage._getDB();return new Promise((t,n)=>{let r=e.transaction(this.storage.storeName,`readwrite`).objectStore(this.storage.storeName).clear();r.onsuccess=()=>{console.log(`[VFS ${this.id}] Successfully wiped browser storage.`),t()},r.onerror=()=>n(r.error)})}}},qt=class{constructor(e){this.id=e,this.reachability=`UNKNOWN`,this.lastPulse=0,this.pps=0,this._pulseCount=0}_tickPPS(){this.pps=this._pulseCount,this._pulseCount=0}async read(e,t,n){throw Error(`Not implemented`)}async spy(e,t,n){throw Error(`Not implemented`)}async subscribe(e,t,n){throw Error(`Not implemented`)}async notify(e,t,n){throw Error(`Not implemented`)}},Jt=class extends qt{constructor(e,t,n,r={}){super(e),this.url=t.replace(/\/$/,``),this.fetch=n,this.localUrl=r.localUrl,this.signal=r.signal}async read(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4}=n,a={"Content-Type":`application/json`,"x-vfs-id":r[0]};this.localUrl&&(a[`x-vfs-local-url`]=this.localUrl);let o=await this.fetch(`${this.url}/read`,{method:`POST`,headers:a,signal:this.signal,body:JSON.stringify({path:e,parameters:t,stack:r,expiresAt:i})});return o.ok&&o.body?{body:o.body,headers:o.headers}:null}async spy(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4}=n,a={"Content-Type":`application/json`,"x-vfs-id":r[0]};this.localUrl&&(a[`x-vfs-local-url`]=this.localUrl);let o=await this.fetch(`${this.url}/spy`,{method:`POST`,headers:a,signal:this.signal,body:JSON.stringify({path:e,parameters:t,stack:r,expiresAt:i})});return o.ok&&o.body?o.body:null}async subscribe(e,t,n){await this.fetch(`${this.url}/subscribe`,{method:`POST`,headers:{"Content-Type":`application/json`,"x-vfs-id":n[n.length-1]},signal:this.signal,body:JSON.stringify({selector:e,expiresAt:t,stack:n})}).catch(()=>{})}async notify(e,t,n=[]){this._pulseCount++,this.lastPulse=Date.now(),await this.fetch(`${this.url}/notify`,{method:`POST`,headers:{"Content-Type":`application/json`},signal:this.signal,body:JSON.stringify({selector:e,payload:t,stack:n})}).catch(()=>{})}},Yt=class extends qt{constructor(e,t){super(e),this.registry=t}async read(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4}=n,a=globalThis.crypto.randomUUID(),o=new Promise((t,n)=>{let r=setTimeout(()=>{this.registry.replies.delete(a),n(Error(`Reverse read timeout for ${e}`))},i-Date.now());this.registry.replies.set(a,(e,n=new Map)=>{clearTimeout(r),t({body:e,headers:n})})});return this.registry.dispatch(this.id,{type:`COMMAND`,op:`READ`,id:a,path:e,parameters:t,stack:r,expiresAt:i})?o:(this.registry.replies.delete(a),null)}async spy(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4}=n,a=globalThis.crypto.randomUUID(),o=new Promise((t,n)=>{let r=setTimeout(()=>{this.registry.replies.delete(a),n(Error(`Reverse spy timeout for ${e}`))},i-Date.now());this.registry.replies.set(a,(e,n=new Map)=>{clearTimeout(r),t({body:e,headers:n})})});return this.registry.dispatch(this.id,{type:`COMMAND`,op:`SPY`,id:a,path:e,parameters:t,stack:r,expiresAt:i})?o:(this.registry.replies.delete(a),null)}async subscribe(e,t,n){this.registry.dispatch(this.id,{type:`COMMAND`,op:`SUB`,selector:e,expiresAt:t,stack:n})}async notify(e,t,n=[]){this._pulseCount++,this.lastPulse=Date.now(),this.registry.dispatch(this.id,{type:`COMMAND`,op:`NOTIFY`,selector:e,payload:t,stack:n})}},Xt=class{constructor(e,t=[],n={}){this.vfs=e,this.fetch=n.fetch||globalThis.fetch.bind(globalThis),this.localUrl=n.localUrl,this.peers=new Map,this.connecting=new Set,this.neighborUrls=t.map(e=>e.replace(/\/$/,``)),this.abortController=new AbortController,this.interests=new Map,this.lastNotify=new Map,this.notifyHistory=[],this.reverseRegistry={listeners:new Map,replies:new Map,dispatch:(e,t)=>{let n=this.reverseRegistry.listeners.get(e);return n?(this.reverseRegistry.listeners.delete(e),n.writeHead(200,{"Content-Type":`application/json`}),n.end(JSON.stringify(t)),!0):!1}},this.vfs.events.on(`trace`,e=>{let t=e.selector||{path:`sys/trace`,parameters:{}};this.notify(t,{...e,peer:this.vfs.id,timestamp:Date.now()})}),this._ppsInterval=setInterval(()=>{for(let e of this.peers.values())e._tickPPS()},1e3),this._ppsInterval.unref&&this._ppsInterval.unref(),this._topoInterval=setInterval(()=>{let e=[...this.peers.values()].map(e=>({id:e.id,reachability:e.reachability})),t={path:`sys/topo`,parameters:{id:this.vfs.id}};this.notify(t,{type:`TOPOLOGY_UPDATE`,peer:this.vfs.id,neighbors:e})},5e3),this._topoInterval.unref&&this._topoInterval.unref()}async subscribe(e,t=Date.now()+6e4,n=[]){let r=[...this.peers.values()].filter(e=>!n.includes(e.id)),i=[...n,this.vfs.id];for(let n of r)try{n.subscribe(e,t,i).catch(()=>{})}catch{}}addInterest(e,t,n,r=[]){let i=JSON.stringify(Bt(t));this.interests.has(i)||this.interests.set(i,{selector:t,subs:new Map});let a=this.interests.get(i),o=a.subs.size===0;a.subs.set(e,n);for(let[e,t]of a.subs.entries())Date.now()>t&&a.subs.delete(e);o&&this.subscribe(t,n,[...r,e]).catch(()=>{})}notify(e,t,n=[]){if(n.includes(this.vfs.id))return;let r=[...n,this.vfs.id],i=Bt(e),a=JSON.stringify(i);this.lastNotify.set(a,t),this.notifyHistory.push({selector:i,payload:{...t,stack:r},t:Date.now()}),this.notifyHistory.length>100&&this.notifyHistory.shift();for(let[e,a]of this.interests.entries())if(this._matches(a.selector,i))for(let[e,o]of a.subs.entries()){if(Date.now()>o){a.subs.delete(e);continue}if(n.includes(e))continue;let s=this.peers.get(e);s&&s.notify(i,t,r).catch(()=>{})}}_matches(e,t){if(!e||!t)return!1;if(e.path===t.path){let n=Object.keys(e.parameters||{});if(n.length===0)return!0;for(let r of n)if(JSON.stringify(e.parameters[r])!==JSON.stringify(t.parameters?.[r]))return!1;return!0}return!1}async start(){this.vfs.mesh=this;for(let e of this.neighborUrls)await this.addPeer(e)}async listenLoop(e,t=null,n=null){if(!this.abortController.signal.aborted){try{let r={"x-vfs-peer-id":this.vfs.id};t&&(r[`x-vfs-reply-to`]=t);let i=await this.fetch(`${e}/listen`,{method:`POST`,headers:r,signal:this.abortController.signal,body:n});if(i.status===200){let t=await i.json();if(t.op===`READ`){let n=await this.vfs.read(t.path,t.parameters,{stack:t.stack,expiresAt:t.expiresAt});return this.listenLoop(e,t.id,n)}else if(t.op===`SPY`){let n=await this.vfs.spy(t.path,t.parameters,{stack:t.stack,expiresAt:t.expiresAt});return this.listenLoop(e,t.id,n)}else if(t.op===`SUB`)return this.addInterest(t.stack[0]||`unknown`,t.selector,t.expiresAt,t.stack),this.listenLoop(e);else if(t.op===`NOTIFY`)return this.notify(t.selector,t.payload,t.stack),this.listenLoop(e)}}catch{if(this.abortController.signal.aborted)return;await new Promise(e=>{let t=setTimeout(e,2e3);this.abortController.signal.addEventListener(`abort`,()=>{clearTimeout(t),e()},{once:!0})})}return this.listenLoop(e)}}async testReachability(e){if(!e)return!1;try{return(await this.fetch(`${e}/health`,{method:`GET`,signal:AbortSignal.timeout(2e3)})).ok}catch{return!1}}async addPeer(e){if(e=e.replace(/\/$/,``),!this.connecting.has(e)){this.connecting.add(e);try{let t=await this.fetch(`${e}/register`,{method:`POST`,headers:{"Content-Type":`application/json`},body:JSON.stringify({id:this.vfs.id,url:this.localUrl}),signal:this.abortController.signal});if(t.ok){let n=await t.json(),r=n.id;if(r&&r!==this.vfs.id){if(!this.peers.has(r)){let t=new Jt(r,e,this.fetch,{localUrl:this.localUrl,signal:this.abortController.signal});t.reachability=`DIRECT`,this.peers.set(r,t);for(let e of this.interests.values())t.subscribe(e.selector,Date.now()+6e4,[this.vfs.id]).catch(()=>{})}return n.reachability===`REVERSE`&&this.listenLoop(e),r}}}catch{}finally{this.connecting.delete(e)}}}registerReversePeer(e,t,n=null,r=null){if(n&&r){let e=this.reverseRegistry.replies.get(n);if(e){this.reverseRegistry.replies.delete(n);let t=new Map;r.length&&t.set(`Content-Length`,r.length.toString()),e(r,t)}}if(!this.peers.has(e)){let t=new Yt(e,this.reverseRegistry);t.reachability=`REVERSE`,this.peers.set(e,t);for(let e of this.interests.values())t.subscribe(e.selector,Date.now()+6e4,[this.vfs.id]).catch(()=>{})}let i=this.reverseRegistry.listeners.get(e);if(i)try{i.writeHead(204),i.end()}catch{}this.reverseRegistry.listeners.set(e,t)}async read(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4}=n,a=[...r,this.vfs.id],o=[...this.peers.values()].filter(e=>!r.includes(e.id));if(o.length===0)return null;let s=o.map(async n=>{try{let r=await n.read(e,t,{stack:a,expiresAt:i});if(r)return r}catch{}throw Error(`Peer failed`)});try{return await Promise.any(s)}catch{return null}}async spy(e,t,n={}){let{stack:r=[],expiresAt:i=Date.now()+3e4}=n,a=[...r,this.vfs.id],o=[...this.peers.values()].filter(e=>!r.includes(e.id));if(o.length===0)return null;let s=o.map(async n=>{try{return await n.spy(e,t,{stack:a,expiresAt:i})}catch{return null}}),c=(await Promise.allSettled(s)).filter(e=>e.status===`fulfilled`&&e.value).map(e=>e.value);return c.length===0?null:new ReadableStream({async start(e){for(let t of c){let n=t.getReader();try{for(;;){let{done:t,value:r}=await n.read();if(t)break;e.enqueue(r)}}finally{n.releaseLock()}}e.close()}})}stop(){this.abortController.abort(),clearInterval(this._ppsInterval),clearInterval(this._topoInterval);for(let e of this.reverseRegistry.listeners.values())try{e.end()}catch{}}},Zt=class extends Xt{},Qt=class{constructor(){this.tokens=[],this.pos=0}parse(e){if(!e||!e.trim())return null;console.log(`[JotParser] Tokenizing:`,e),this.tokens=this._tokenize(e),console.log(`[JotParser] Tokens:`,this.tokens),this.pos=0;let t=this._parseExpression();if(console.log(`[JotParser] AST:`,JSON.stringify(t,null,2)),this.pos<this.tokens.length)throw Error(`Unexpected token at end: ${this.tokens[this.pos]}`);return t}_tokenize(e){let t=[],n=e.replace(/\/\/.*$/gm,``),r=/\s*([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+(?:\.[0-9]+)?|"[^"]*"|'[^']*'|\.\.\.|\.|\(|\)|\{|\}|:|\[|\]|,)\s*/g,i;for(;(i=r.exec(n))!==null;)t.push(i[1]);return t}_peek(){return this.tokens[this.pos]}_consume(e){if(this.pos>=this.tokens.length)throw Error(`Expected ${e||`token`}, got end of input`);let t=this.tokens[this.pos++];if(e&&t!==e)throw Error(`Expected ${e}, got ${t}`);return t}_parseExpression(){let e=this._parsePrimary();for(;this._peek()===`.`;){this._consume(`.`);let t=this._consume();if(this._peek()!==`(`)throw Error(`Expected ( after method name, got ${this._peek()}`);let n=this._parseArguments();e=this._wrapInOp(t,e,n)}return e}_parsePrimary(){let e=this._peek();if(!e)throw Error(`Unexpected end of input`);if(e===`[`)return this._parseArray();if(e===`{`)return this._parseObject();if(e===`(`){this._consume(`(`);let e=this._parseExpression();return this._consume(`)`),e}if(/^[0-9]/.test(e))return parseFloat(this._consume());if(/^["']/.test(e))return this._consume().slice(1,-1);if(/^[a-zA-Z_]/.test(e)){let e=this._consume();if(this._peek()===`(`){let t=this._parseArguments();return this._createSelector(e,t)}return{type:`SYMBOL`,name:e}}throw Error(`Unexpected token: ${e}`)}_parseArray(){this._consume(`[`);let e=[];for(;this._peek()&&this._peek()!==`]`;)e.push(this._parseExpression()),this._peek()===`,`&&this._consume(`,`);return this._consume(`]`),e}_parseObject(){this._consume(`{`);let e={};for(;this._peek()&&this._peek()!==`}`;){let t=this._consume();this._consume(`:`),e[t]=this._parseExpression(),this._peek()===`,`&&this._consume(`,`)}return this._consume(`}`),e}_parseArguments(){this._consume(`(`);let e=[];for(;this._peek()&&this._peek()!==`)`;)e.push(this._parseExpression()),this._peek()===`,`&&this._consume(`,`);return this._consume(`)`),e}_createSelector(e,t){let n=`op/${e}`,r={};if(t.length===1&&typeof t[0]==`object`&&t[0].type!==`SYMBOL`&&!Array.isArray(t[0])&&(r=t[0]),e===`hexagon`){n=`shape/hexagon/${r.variant||`full`}`;let{variant:e,...t}=r;r=t}else e===`tri`||e===`triangle`?r.side===void 0?r.angle===void 0?r.a!==void 0&&r.b!==void 0&&r.c!==void 0?n=`shape/triangle/sss`:(n=`shape/triangle/equilateral`,r.side===void 0&&(r.side=10)):n=`shape/triangle/sas`:n=`shape/triangle/equilateral`:e===`box`?(n=`shape/box`,t.length>=1&&typeof t[0]!=`object`&&(r.width=t[0]),t.length>=2&&typeof t[1]!=`object`&&(r.height=t[1]),t.length>=3&&typeof t[2]!=`object`&&(r.depth=t[2])):e===`pt`?(n=`shape/point`,t.length>=1&&typeof t[0]!=`object`&&(r.x=t[0]),t.length>=2&&typeof t[1]!=`object`&&(r.y=t[1]),t.length>=3&&typeof t[2]!=`object`&&(r.z=t[2])):e===`arc`?n=`shape/arc`:e===`orb`?n=`shape/orb`:e===`origin`&&(n=`shape/origin`);let i=Bt(n,r);return console.log(`[JotParser] Constructed selector for ${e}:`,JSON.stringify(i,null,2)),i}_wrapInOp(e,t,n){let r={rx:`op/rotateX`,ry:`op/rotateY`,rz:`op/rotateZ`,move:`op/move`,at:`op/move`,size:`op/size`,sz:`op/size`,extrude:`op/extrude`,offset:`op/offset`,outline:`op/outline`,points:`op/points`,spec:`op/spec`}[e]||`op/${e}`,i={source:t};return e===`spec`?i.spec=n[0]:e===`rx`||e===`rotateX`||e===`ry`||e===`rotateY`||e===`rz`||e===`rotateZ`?i.turns=n[0]:e===`offset`?i.radius=n[0]:e===`extrude`?i.height=n[0]:n.length>0&&(i.args=n),Bt(r,i)}},$t=class{resolve(e,t={}){if(e&&typeof e==`object`){if(e.path===`op/spec`){let n=e.parameters.spec,r=e.parameters.source,i=this.canonicalize(n,t);return this.resolve(r,i)}if(e.type===`SYMBOL`)return t[e.name]===void 0?e:t[e.name];if(Array.isArray(e))return e.map(e=>this.resolve(e,t));if(e.path&&e.parameters){let n=this.resolve(e.parameters,t);return Bt(e.path,n)}let n={};for(let[r,i]of Object.entries(e))n[r]=this.resolve(i,t);return n}return e}canonicalize(e,t={}){let n={...t};for(let[r,i]of Object.entries(e)){if(i.alias){let e=Array.isArray(i.alias)?i.alias:[i.alias];for(let i of e)t[i]!==void 0&&n[r]===void 0&&(n[r]=t[i])}n[r]===void 0&&i.default!==void 0&&(n[r]=i.default)}return n}};function en(e){let t=new Qt,n=new $t;e.registerProvider(`jot/eval`,async(e,r,i)=>{let{expression:a,params:o={}}=r.parameters;if(!a)return null;try{let r=t.parse(a),s=n.resolve(r,o);return Array.isArray(s)?await e.read(`op/group`,{sources:s},i):await e.read(s.path,s.parameters,i)}catch(e){return console.error(`[JotProvider] Evaluation error:`,e),null}}),e.registerProvider(`.jot`,async(e,t,n)=>{if(!t.path.endsWith(`.jot`))return null;try{let r=await e.readText(t.path,{},n);return r?await e.read(`jot/eval`,{expression:r,params:t.parameters},n):null}catch(e){return console.error(`[JotProvider] File error for ${t.path}:`,e),null}})}var tn=new Kt({id:`ui-main`,storage:new Gt});en(tn);var nn=`http://${window.location.hostname}:9092`,rn=new Zt(tn,[nn]),[an,on]=M({}),[sn,cn]=M({}),[ln,un]=M([]),[dn,fn]=M({peers:[]}),[pn,mn]=M({}),[hn,gn]=M(!1),[_n,vn]=M(`idle`),yn={vfs:tn,graph:an,schemas:sn,pulse:ln,meshTopology:dn,meshPositions:pn,setMeshPositions:mn,isConnected:hn,discoveryStatus:_n,async start(){console.log(`[UX] Mesh-VFS initialized. Connecting to: ${nn}`),tn.events.on(`state`,async e=>{e.path&&on(t=>({...t,[e.cid]:{...t[e.cid],...e}}))});let e=new Map,t=rn.notify.bind(rn);rn.notify=(n,r,i)=>{t(n,r,i),r.type===`TOPOLOGY_UPDATE`&&e.set(r.peer,r.neighbors),un(e=>[...e,{selector:n,payload:r,t:Date.now()}].slice(-20))};try{await rn.fetch(`${nn}/health`),gn(!0)}catch{gn(!1)}await rn.start(),gn(!0),tn.read(`sys/topo`,{},{followLinks:!1}).catch(()=>{}),setInterval(()=>{let t=new Map;t.set(tn.id,{id:tn.id,type:`BROWSER`,pps:0,neighbors:[]});for(let[n,r]of e.entries())t.has(n)?t.get(n).neighbors=r:t.set(n,{id:n,type:`PEER`,pps:0,neighbors:r});for(let e of rn.peers.values())if(t.has(e.id)){let n=t.get(e.id);n.pps=e.pps,n.reachability=e.reachability}fn({peers:[...t.values()]})},1e3),this.discoverSchemas()},async discoverSchemas(){vn(`loading`);try{let e=await tn.spy(`sys/schema`,{});if(!e){vn(`empty`);return}let t=0;for await(let n of Kt.parseVFSBundle(e))try{let e=JSON.parse(new TextDecoder().decode(n.data)),r=n.selector.parameters.target;e._origin=n.selector.parameters.provider||`unknown`,r&&e&&(t++,tn.addSchema(r,e),cn(t=>({...t,[r]:e})))}catch{}vn(t>0?`success`:`empty`)}catch{vn(`error`)}},stop(){rn.stop(),tn.close()},async read(e,t={}){return tn.read(e,t)},async write(e,t={},n){return tn.writeData(e,t,n)}};typeof window<`u`&&(window.blackboard=yn);var bn={LEFT:0,MIDDLE:1,RIGHT:2,ROTATE:0,DOLLY:1,PAN:2},xn={ROTATE:0,PAN:1,DOLLY_PAN:2,DOLLY_ROTATE:3},Sn=1e3,Cn=1001,wn=1002,Tn=1003,En=1004,Dn=1005,On=1006,kn=1007,An=1008,jn=1009,Mn=1014,Nn=1015,Pn=1016,Fn=1020,In=1023,Ln=1026,Rn=1027,zn=2300,Bn=2301,Vn=2302,Hn=2400,Un=2401,Wn=2402,Gn=3200,Kn=3201,qn=`srgb`,Jn=`srgb-linear`,Yn=`display-p3`,Xn=`display-p3-linear`,Zn=`linear`,Qn=`srgb`,$n=`rec709`,er=7680,tr=35044,nr=1035,rr=2e3,ir=class{addEventListener(e,t){this._listeners===void 0&&(this._listeners={});let n=this._listeners;n[e]===void 0&&(n[e]=[]),n[e].indexOf(t)===-1&&n[e].push(t)}hasEventListener(e,t){if(this._listeners===void 0)return!1;let n=this._listeners;return n[e]!==void 0&&n[e].indexOf(t)!==-1}removeEventListener(e,t){if(this._listeners===void 0)return;let n=this._listeners[e];if(n!==void 0){let e=n.indexOf(t);e!==-1&&n.splice(e,1)}}dispatchEvent(e){if(this._listeners===void 0)return;let t=this._listeners[e.type];if(t!==void 0){e.target=this;let n=t.slice(0);for(let t=0,r=n.length;t<r;t++)n[t].call(this,e);e.target=null}}},ar=`00.01.02.03.04.05.06.07.08.09.0a.0b.0c.0d.0e.0f.10.11.12.13.14.15.16.17.18.19.1a.1b.1c.1d.1e.1f.20.21.22.23.24.25.26.27.28.29.2a.2b.2c.2d.2e.2f.30.31.32.33.34.35.36.37.38.39.3a.3b.3c.3d.3e.3f.40.41.42.43.44.45.46.47.48.49.4a.4b.4c.4d.4e.4f.50.51.52.53.54.55.56.57.58.59.5a.5b.5c.5d.5e.5f.60.61.62.63.64.65.66.67.68.69.6a.6b.6c.6d.6e.6f.70.71.72.73.74.75.76.77.78.79.7a.7b.7c.7d.7e.7f.80.81.82.83.84.85.86.87.88.89.8a.8b.8c.8d.8e.8f.90.91.92.93.94.95.96.97.98.99.9a.9b.9c.9d.9e.9f.a0.a1.a2.a3.a4.a5.a6.a7.a8.a9.aa.ab.ac.ad.ae.af.b0.b1.b2.b3.b4.b5.b6.b7.b8.b9.ba.bb.bc.bd.be.bf.c0.c1.c2.c3.c4.c5.c6.c7.c8.c9.ca.cb.cc.cd.ce.cf.d0.d1.d2.d3.d4.d5.d6.d7.d8.d9.da.db.dc.dd.de.df.e0.e1.e2.e3.e4.e5.e6.e7.e8.e9.ea.eb.ec.ed.ee.ef.f0.f1.f2.f3.f4.f5.f6.f7.f8.f9.fa.fb.fc.fd.fe.ff`.split(`.`),or=1234567,sr=Math.PI/180,cr=180/Math.PI;function lr(){let e=Math.random()*4294967295|0,t=Math.random()*4294967295|0,n=Math.random()*4294967295|0,r=Math.random()*4294967295|0;return(ar[e&255]+ar[e>>8&255]+ar[e>>16&255]+ar[e>>24&255]+`-`+ar[t&255]+ar[t>>8&255]+`-`+ar[t>>16&15|64]+ar[t>>24&255]+`-`+ar[n&63|128]+ar[n>>8&255]+`-`+ar[n>>16&255]+ar[n>>24&255]+ar[r&255]+ar[r>>8&255]+ar[r>>16&255]+ar[r>>24&255]).toLowerCase()}function ur(e,t,n){return Math.max(t,Math.min(n,e))}function dr(e,t){return(e%t+t)%t}function fr(e,t,n,r,i){return r+(e-t)*(i-r)/(n-t)}function pr(e,t,n){return e===t?0:(n-e)/(t-e)}function mr(e,t,n){return(1-n)*e+n*t}function hr(e,t,n,r){return mr(e,t,1-Math.exp(-n*r))}function gr(e,t=1){return t-Math.abs(dr(e,t*2)-t)}function _r(e,t,n){return e<=t?0:e>=n?1:(e=(e-t)/(n-t),e*e*(3-2*e))}function vr(e,t,n){return e<=t?0:e>=n?1:(e=(e-t)/(n-t),e*e*e*(e*(e*6-15)+10))}function yr(e,t){return e+Math.floor(Math.random()*(t-e+1))}function br(e,t){return e+Math.random()*(t-e)}function xr(e){return e*(.5-Math.random())}function Sr(e){e!==void 0&&(or=e);let t=or+=1831565813;return t=Math.imul(t^t>>>15,t|1),t^=t+Math.imul(t^t>>>7,t|61),((t^t>>>14)>>>0)/4294967296}function Cr(e){return e*sr}function wr(e){return e*cr}function Tr(e){return(e&e-1)==0&&e!==0}function Er(e){return 2**Math.ceil(Math.log(e)/Math.LN2)}function Dr(e){return 2**Math.floor(Math.log(e)/Math.LN2)}function Or(e,t,n,r,i){let a=Math.cos,o=Math.sin,s=a(n/2),c=o(n/2),l=a((t+r)/2),u=o((t+r)/2),d=a((t-r)/2),f=o((t-r)/2),p=a((r-t)/2),m=o((r-t)/2);switch(i){case`XYX`:e.set(s*u,c*d,c*f,s*l);break;case`YZY`:e.set(c*f,s*u,c*d,s*l);break;case`ZXZ`:e.set(c*d,c*f,s*u,s*l);break;case`XZX`:e.set(s*u,c*m,c*p,s*l);break;case`YXY`:e.set(c*p,s*u,c*m,s*l);break;case`ZYZ`:e.set(c*m,c*p,s*u,s*l);break;default:console.warn(`THREE.MathUtils: .setQuaternionFromProperEuler() encountered an unknown order: `+i)}}function kr(e,t){switch(t.constructor){case Float32Array:return e;case Uint32Array:return e/4294967295;case Uint16Array:return e/65535;case Uint8Array:return e/255;case Int32Array:return Math.max(e/2147483647,-1);case Int16Array:return Math.max(e/32767,-1);case Int8Array:return Math.max(e/127,-1);default:throw Error(`Invalid component type.`)}}function Ar(e,t){switch(t.constructor){case Float32Array:return e;case Uint32Array:return Math.round(e*4294967295);case Uint16Array:return Math.round(e*65535);case Uint8Array:return Math.round(e*255);case Int32Array:return Math.round(e*2147483647);case Int16Array:return Math.round(e*32767);case Int8Array:return Math.round(e*127);default:throw Error(`Invalid component type.`)}}var jr={DEG2RAD:sr,RAD2DEG:cr,generateUUID:lr,clamp:ur,euclideanModulo:dr,mapLinear:fr,inverseLerp:pr,lerp:mr,damp:hr,pingpong:gr,smoothstep:_r,smootherstep:vr,randInt:yr,randFloat:br,randFloatSpread:xr,seededRandom:Sr,degToRad:Cr,radToDeg:wr,isPowerOfTwo:Tr,ceilPowerOfTwo:Er,floorPowerOfTwo:Dr,setQuaternionFromProperEuler:Or,normalize:Ar,denormalize:kr},Y=class e{constructor(t=0,n=0){e.prototype.isVector2=!0,this.x=t,this.y=n}get width(){return this.x}set width(e){this.x=e}get height(){return this.y}set height(e){this.y=e}set(e,t){return this.x=e,this.y=t,this}setScalar(e){return this.x=e,this.y=e,this}setX(e){return this.x=e,this}setY(e){return this.y=e,this}setComponent(e,t){switch(e){case 0:this.x=t;break;case 1:this.y=t;break;default:throw Error(`index is out of range: `+e)}return this}getComponent(e){switch(e){case 0:return this.x;case 1:return this.y;default:throw Error(`index is out of range: `+e)}}clone(){return new this.constructor(this.x,this.y)}copy(e){return this.x=e.x,this.y=e.y,this}add(e){return this.x+=e.x,this.y+=e.y,this}addScalar(e){return this.x+=e,this.y+=e,this}addVectors(e,t){return this.x=e.x+t.x,this.y=e.y+t.y,this}addScaledVector(e,t){return this.x+=e.x*t,this.y+=e.y*t,this}sub(e){return this.x-=e.x,this.y-=e.y,this}subScalar(e){return this.x-=e,this.y-=e,this}subVectors(e,t){return this.x=e.x-t.x,this.y=e.y-t.y,this}multiply(e){return this.x*=e.x,this.y*=e.y,this}multiplyScalar(e){return this.x*=e,this.y*=e,this}divide(e){return this.x/=e.x,this.y/=e.y,this}divideScalar(e){return this.multiplyScalar(1/e)}applyMatrix3(e){let t=this.x,n=this.y,r=e.elements;return this.x=r[0]*t+r[3]*n+r[6],this.y=r[1]*t+r[4]*n+r[7],this}min(e){return this.x=Math.min(this.x,e.x),this.y=Math.min(this.y,e.y),this}max(e){return this.x=Math.max(this.x,e.x),this.y=Math.max(this.y,e.y),this}clamp(e,t){return this.x=Math.max(e.x,Math.min(t.x,this.x)),this.y=Math.max(e.y,Math.min(t.y,this.y)),this}clampScalar(e,t){return this.x=Math.max(e,Math.min(t,this.x)),this.y=Math.max(e,Math.min(t,this.y)),this}clampLength(e,t){let n=this.length();return this.divideScalar(n||1).multiplyScalar(Math.max(e,Math.min(t,n)))}floor(){return this.x=Math.floor(this.x),this.y=Math.floor(this.y),this}ceil(){return this.x=Math.ceil(this.x),this.y=Math.ceil(this.y),this}round(){return this.x=Math.round(this.x),this.y=Math.round(this.y),this}roundToZero(){return this.x=Math.trunc(this.x),this.y=Math.trunc(this.y),this}negate(){return this.x=-this.x,this.y=-this.y,this}dot(e){return this.x*e.x+this.y*e.y}cross(e){return this.x*e.y-this.y*e.x}lengthSq(){return this.x*this.x+this.y*this.y}length(){return Math.sqrt(this.x*this.x+this.y*this.y)}manhattanLength(){return Math.abs(this.x)+Math.abs(this.y)}normalize(){return this.divideScalar(this.length()||1)}angle(){return Math.atan2(-this.y,-this.x)+Math.PI}angleTo(e){let t=Math.sqrt(this.lengthSq()*e.lengthSq());if(t===0)return Math.PI/2;let n=this.dot(e)/t;return Math.acos(ur(n,-1,1))}distanceTo(e){return Math.sqrt(this.distanceToSquared(e))}distanceToSquared(e){let t=this.x-e.x,n=this.y-e.y;return t*t+n*n}manhattanDistanceTo(e){return Math.abs(this.x-e.x)+Math.abs(this.y-e.y)}setLength(e){return this.normalize().multiplyScalar(e)}lerp(e,t){return this.x+=(e.x-this.x)*t,this.y+=(e.y-this.y)*t,this}lerpVectors(e,t,n){return this.x=e.x+(t.x-e.x)*n,this.y=e.y+(t.y-e.y)*n,this}equals(e){return e.x===this.x&&e.y===this.y}fromArray(e,t=0){return this.x=e[t],this.y=e[t+1],this}toArray(e=[],t=0){return e[t]=this.x,e[t+1]=this.y,e}fromBufferAttribute(e,t){return this.x=e.getX(t),this.y=e.getY(t),this}rotateAround(e,t){let n=Math.cos(t),r=Math.sin(t),i=this.x-e.x,a=this.y-e.y;return this.x=i*n-a*r+e.x,this.y=i*r+a*n+e.y,this}random(){return this.x=Math.random(),this.y=Math.random(),this}*[Symbol.iterator](){yield this.x,yield this.y}},X=class e{constructor(t,n,r,i,a,o,s,c,l){e.prototype.isMatrix3=!0,this.elements=[1,0,0,0,1,0,0,0,1],t!==void 0&&this.set(t,n,r,i,a,o,s,c,l)}set(e,t,n,r,i,a,o,s,c){let l=this.elements;return l[0]=e,l[1]=r,l[2]=o,l[3]=t,l[4]=i,l[5]=s,l[6]=n,l[7]=a,l[8]=c,this}identity(){return this.set(1,0,0,0,1,0,0,0,1),this}copy(e){let t=this.elements,n=e.elements;return t[0]=n[0],t[1]=n[1],t[2]=n[2],t[3]=n[3],t[4]=n[4],t[5]=n[5],t[6]=n[6],t[7]=n[7],t[8]=n[8],this}extractBasis(e,t,n){return e.setFromMatrix3Column(this,0),t.setFromMatrix3Column(this,1),n.setFromMatrix3Column(this,2),this}setFromMatrix4(e){let t=e.elements;return this.set(t[0],t[4],t[8],t[1],t[5],t[9],t[2],t[6],t[10]),this}multiply(e){return this.multiplyMatrices(this,e)}premultiply(e){return this.multiplyMatrices(e,this)}multiplyMatrices(e,t){let n=e.elements,r=t.elements,i=this.elements,a=n[0],o=n[3],s=n[6],c=n[1],l=n[4],u=n[7],d=n[2],f=n[5],p=n[8],m=r[0],h=r[3],g=r[6],_=r[1],v=r[4],y=r[7],b=r[2],x=r[5],S=r[8];return i[0]=a*m+o*_+s*b,i[3]=a*h+o*v+s*x,i[6]=a*g+o*y+s*S,i[1]=c*m+l*_+u*b,i[4]=c*h+l*v+u*x,i[7]=c*g+l*y+u*S,i[2]=d*m+f*_+p*b,i[5]=d*h+f*v+p*x,i[8]=d*g+f*y+p*S,this}multiplyScalar(e){let t=this.elements;return t[0]*=e,t[3]*=e,t[6]*=e,t[1]*=e,t[4]*=e,t[7]*=e,t[2]*=e,t[5]*=e,t[8]*=e,this}determinant(){let e=this.elements,t=e[0],n=e[1],r=e[2],i=e[3],a=e[4],o=e[5],s=e[6],c=e[7],l=e[8];return t*a*l-t*o*c-n*i*l+n*o*s+r*i*c-r*a*s}invert(){let e=this.elements,t=e[0],n=e[1],r=e[2],i=e[3],a=e[4],o=e[5],s=e[6],c=e[7],l=e[8],u=l*a-o*c,d=o*s-l*i,f=c*i-a*s,p=t*u+n*d+r*f;if(p===0)return this.set(0,0,0,0,0,0,0,0,0);let m=1/p;return e[0]=u*m,e[1]=(r*c-l*n)*m,e[2]=(o*n-r*a)*m,e[3]=d*m,e[4]=(l*t-r*s)*m,e[5]=(r*i-o*t)*m,e[6]=f*m,e[7]=(n*s-c*t)*m,e[8]=(a*t-n*i)*m,this}transpose(){let e,t=this.elements;return e=t[1],t[1]=t[3],t[3]=e,e=t[2],t[2]=t[6],t[6]=e,e=t[5],t[5]=t[7],t[7]=e,this}getNormalMatrix(e){return this.setFromMatrix4(e).invert().transpose()}transposeIntoArray(e){let t=this.elements;return e[0]=t[0],e[1]=t[3],e[2]=t[6],e[3]=t[1],e[4]=t[4],e[5]=t[7],e[6]=t[2],e[7]=t[5],e[8]=t[8],this}setUvTransform(e,t,n,r,i,a,o){let s=Math.cos(i),c=Math.sin(i);return this.set(n*s,n*c,-n*(s*a+c*o)+a+e,-r*c,r*s,-r*(-c*a+s*o)+o+t,0,0,1),this}scale(e,t){return this.premultiply(Mr.makeScale(e,t)),this}rotate(e){return this.premultiply(Mr.makeRotation(-e)),this}translate(e,t){return this.premultiply(Mr.makeTranslation(e,t)),this}makeTranslation(e,t){return e.isVector2?this.set(1,0,e.x,0,1,e.y,0,0,1):this.set(1,0,e,0,1,t,0,0,1),this}makeRotation(e){let t=Math.cos(e),n=Math.sin(e);return this.set(t,-n,0,n,t,0,0,0,1),this}makeScale(e,t){return this.set(e,0,0,0,t,0,0,0,1),this}equals(e){let t=this.elements,n=e.elements;for(let e=0;e<9;e++)if(t[e]!==n[e])return!1;return!0}fromArray(e,t=0){for(let n=0;n<9;n++)this.elements[n]=e[n+t];return this}toArray(e=[],t=0){let n=this.elements;return e[t]=n[0],e[t+1]=n[1],e[t+2]=n[2],e[t+3]=n[3],e[t+4]=n[4],e[t+5]=n[5],e[t+6]=n[6],e[t+7]=n[7],e[t+8]=n[8],e}clone(){return new this.constructor().fromArray(this.elements)}},Mr=new X;function Nr(e){for(let t=e.length-1;t>=0;--t)if(e[t]>=65535)return!0;return!1}function Pr(e){return document.createElementNS(`http://www.w3.org/1999/xhtml`,e)}function Fr(){let e=Pr(`canvas`);return e.style.display=`block`,e}var Ir={};function Lr(e){e in Ir||(Ir[e]=!0,console.warn(e))}var Rr=new X().set(.8224621,.177538,0,.0331941,.9668058,0,.0170827,.0723974,.9105199),zr=new X().set(1.2249401,-.2249404,0,-.0420569,1.0420571,0,-.0196376,-.0786361,1.0982735),Br={[Jn]:{transfer:Zn,primaries:$n,toReference:e=>e,fromReference:e=>e},[qn]:{transfer:Qn,primaries:$n,toReference:e=>e.convertSRGBToLinear(),fromReference:e=>e.convertLinearToSRGB()},[Xn]:{transfer:Zn,primaries:`p3`,toReference:e=>e.applyMatrix3(zr),fromReference:e=>e.applyMatrix3(Rr)},[Yn]:{transfer:Qn,primaries:`p3`,toReference:e=>e.convertSRGBToLinear().applyMatrix3(zr),fromReference:e=>e.applyMatrix3(Rr).convertLinearToSRGB()}},Vr=new Set([Jn,Xn]),Hr={enabled:!0,_workingColorSpace:Jn,get workingColorSpace(){return this._workingColorSpace},set workingColorSpace(e){if(!Vr.has(e))throw Error(`Unsupported working color space, "${e}".`);this._workingColorSpace=e},convert:function(e,t,n){if(this.enabled===!1||t===n||!t||!n)return e;let r=Br[t].toReference,i=Br[n].fromReference;return i(r(e))},fromWorkingColorSpace:function(e,t){return this.convert(e,this._workingColorSpace,t)},toWorkingColorSpace:function(e,t){return this.convert(e,t,this._workingColorSpace)},getPrimaries:function(e){return Br[e].primaries},getTransfer:function(e){return e===``?Zn:Br[e].transfer}};function Ur(e){return e<.04045?e*.0773993808:(e*.9478672986+.0521327014)**2.4}function Wr(e){return e<.0031308?e*12.92:1.055*e**.41666-.055}var Gr,Kr=class{static getDataURL(e){if(/^data:/i.test(e.src)||typeof HTMLCanvasElement>`u`)return e.src;let t;if(e instanceof HTMLCanvasElement)t=e;else{Gr===void 0&&(Gr=Pr(`canvas`)),Gr.width=e.width,Gr.height=e.height;let n=Gr.getContext(`2d`);e instanceof ImageData?n.putImageData(e,0,0):n.drawImage(e,0,0,e.width,e.height),t=Gr}return t.width>2048||t.height>2048?(console.warn(`THREE.ImageUtils.getDataURL: Image converted to jpg for performance reasons`,e),t.toDataURL(`image/jpeg`,.6)):t.toDataURL(`image/png`)}static sRGBToLinear(e){if(typeof HTMLImageElement<`u`&&e instanceof HTMLImageElement||typeof HTMLCanvasElement<`u`&&e instanceof HTMLCanvasElement||typeof ImageBitmap<`u`&&e instanceof ImageBitmap){let t=Pr(`canvas`);t.width=e.width,t.height=e.height;let n=t.getContext(`2d`);n.drawImage(e,0,0,e.width,e.height);let r=n.getImageData(0,0,e.width,e.height),i=r.data;for(let e=0;e<i.length;e++)i[e]=Ur(i[e]/255)*255;return n.putImageData(r,0,0),t}else if(e.data){let t=e.data.slice(0);for(let e=0;e<t.length;e++)t instanceof Uint8Array||t instanceof Uint8ClampedArray?t[e]=Math.floor(Ur(t[e]/255)*255):t[e]=Ur(t[e]);return{data:t,width:e.width,height:e.height}}else return console.warn(`THREE.ImageUtils.sRGBToLinear(): Unsupported image type. No color space conversion applied.`),e}},qr=0,Jr=class{constructor(e=null){this.isSource=!0,Object.defineProperty(this,`id`,{value:qr++}),this.uuid=lr(),this.data=e,this.dataReady=!0,this.version=0}set needsUpdate(e){e===!0&&this.version++}toJSON(e){let t=e===void 0||typeof e==`string`;if(!t&&e.images[this.uuid]!==void 0)return e.images[this.uuid];let n={uuid:this.uuid,url:``},r=this.data;if(r!==null){let e;if(Array.isArray(r)){e=[];for(let t=0,n=r.length;t<n;t++)r[t].isDataTexture?e.push(Yr(r[t].image)):e.push(Yr(r[t]))}else e=Yr(r);n.url=e}return t||(e.images[this.uuid]=n),n}};function Yr(e){return typeof HTMLImageElement<`u`&&e instanceof HTMLImageElement||typeof HTMLCanvasElement<`u`&&e instanceof HTMLCanvasElement||typeof ImageBitmap<`u`&&e instanceof ImageBitmap?Kr.getDataURL(e):e.data?{data:Array.from(e.data),width:e.width,height:e.height,type:e.data.constructor.name}:(console.warn(`THREE.Texture: Unable to serialize Texture.`),{})}var Xr=0,Zr=class e extends ir{constructor(t=e.DEFAULT_IMAGE,n=e.DEFAULT_MAPPING,r=Cn,i=Cn,a=On,o=An,s=In,c=jn,l=e.DEFAULT_ANISOTROPY,u=``){super(),this.isTexture=!0,Object.defineProperty(this,`id`,{value:Xr++}),this.uuid=lr(),this.name=``,this.source=new Jr(t),this.mipmaps=[],this.mapping=n,this.channel=0,this.wrapS=r,this.wrapT=i,this.magFilter=a,this.minFilter=o,this.anisotropy=l,this.format=s,this.internalFormat=null,this.type=c,this.offset=new Y(0,0),this.repeat=new Y(1,1),this.center=new Y(0,0),this.rotation=0,this.matrixAutoUpdate=!0,this.matrix=new X,this.generateMipmaps=!0,this.premultiplyAlpha=!1,this.flipY=!0,this.unpackAlignment=4,this.colorSpace=u,this.userData={},this.version=0,this.onUpdate=null,this.isRenderTargetTexture=!1,this.needsPMREMUpdate=!1}get image(){return this.source.data}set image(e=null){this.source.data=e}updateMatrix(){this.matrix.setUvTransform(this.offset.x,this.offset.y,this.repeat.x,this.repeat.y,this.rotation,this.center.x,this.center.y)}clone(){return new this.constructor().copy(this)}copy(e){return this.name=e.name,this.source=e.source,this.mipmaps=e.mipmaps.slice(0),this.mapping=e.mapping,this.channel=e.channel,this.wrapS=e.wrapS,this.wrapT=e.wrapT,this.magFilter=e.magFilter,this.minFilter=e.minFilter,this.anisotropy=e.anisotropy,this.format=e.format,this.internalFormat=e.internalFormat,this.type=e.type,this.offset.copy(e.offset),this.repeat.copy(e.repeat),this.center.copy(e.center),this.rotation=e.rotation,this.matrixAutoUpdate=e.matrixAutoUpdate,this.matrix.copy(e.matrix),this.generateMipmaps=e.generateMipmaps,this.premultiplyAlpha=e.premultiplyAlpha,this.flipY=e.flipY,this.unpackAlignment=e.unpackAlignment,this.colorSpace=e.colorSpace,this.userData=JSON.parse(JSON.stringify(e.userData)),this.needsUpdate=!0,this}toJSON(e){let t=e===void 0||typeof e==`string`;if(!t&&e.textures[this.uuid]!==void 0)return e.textures[this.uuid];let n={metadata:{version:4.6,type:`Texture`,generator:`Texture.toJSON`},uuid:this.uuid,name:this.name,image:this.source.toJSON(e).uuid,mapping:this.mapping,channel:this.channel,repeat:[this.repeat.x,this.repeat.y],offset:[this.offset.x,this.offset.y],center:[this.center.x,this.center.y],rotation:this.rotation,wrap:[this.wrapS,this.wrapT],format:this.format,internalFormat:this.internalFormat,type:this.type,colorSpace:this.colorSpace,minFilter:this.minFilter,magFilter:this.magFilter,anisotropy:this.anisotropy,flipY:this.flipY,generateMipmaps:this.generateMipmaps,premultiplyAlpha:this.premultiplyAlpha,unpackAlignment:this.unpackAlignment};return Object.keys(this.userData).length>0&&(n.userData=this.userData),t||(e.textures[this.uuid]=n),n}dispose(){this.dispatchEvent({type:`dispose`})}transformUv(e){if(this.mapping!==300)return e;if(e.applyMatrix3(this.matrix),e.x<0||e.x>1)switch(this.wrapS){case Sn:e.x-=Math.floor(e.x);break;case Cn:e.x=e.x<0?0:1;break;case wn:Math.abs(Math.floor(e.x)%2)===1?e.x=Math.ceil(e.x)-e.x:e.x-=Math.floor(e.x);break}if(e.y<0||e.y>1)switch(this.wrapT){case Sn:e.y-=Math.floor(e.y);break;case Cn:e.y=e.y<0?0:1;break;case wn:Math.abs(Math.floor(e.y)%2)===1?e.y=Math.ceil(e.y)-e.y:e.y-=Math.floor(e.y);break}return this.flipY&&(e.y=1-e.y),e}set needsUpdate(e){e===!0&&(this.version++,this.source.needsUpdate=!0)}};Zr.DEFAULT_IMAGE=null,Zr.DEFAULT_MAPPING=300,Zr.DEFAULT_ANISOTROPY=1;var Qr=class e{constructor(t=0,n=0,r=0,i=1){e.prototype.isVector4=!0,this.x=t,this.y=n,this.z=r,this.w=i}get width(){return this.z}set width(e){this.z=e}get height(){return this.w}set height(e){this.w=e}set(e,t,n,r){return this.x=e,this.y=t,this.z=n,this.w=r,this}setScalar(e){return this.x=e,this.y=e,this.z=e,this.w=e,this}setX(e){return this.x=e,this}setY(e){return this.y=e,this}setZ(e){return this.z=e,this}setW(e){return this.w=e,this}setComponent(e,t){switch(e){case 0:this.x=t;break;case 1:this.y=t;break;case 2:this.z=t;break;case 3:this.w=t;break;default:throw Error(`index is out of range: `+e)}return this}getComponent(e){switch(e){case 0:return this.x;case 1:return this.y;case 2:return this.z;case 3:return this.w;default:throw Error(`index is out of range: `+e)}}clone(){return new this.constructor(this.x,this.y,this.z,this.w)}copy(e){return this.x=e.x,this.y=e.y,this.z=e.z,this.w=e.w===void 0?1:e.w,this}add(e){return this.x+=e.x,this.y+=e.y,this.z+=e.z,this.w+=e.w,this}addScalar(e){return this.x+=e,this.y+=e,this.z+=e,this.w+=e,this}addVectors(e,t){return this.x=e.x+t.x,this.y=e.y+t.y,this.z=e.z+t.z,this.w=e.w+t.w,this}addScaledVector(e,t){return this.x+=e.x*t,this.y+=e.y*t,this.z+=e.z*t,this.w+=e.w*t,this}sub(e){return this.x-=e.x,this.y-=e.y,this.z-=e.z,this.w-=e.w,this}subScalar(e){return this.x-=e,this.y-=e,this.z-=e,this.w-=e,this}subVectors(e,t){return this.x=e.x-t.x,this.y=e.y-t.y,this.z=e.z-t.z,this.w=e.w-t.w,this}multiply(e){return this.x*=e.x,this.y*=e.y,this.z*=e.z,this.w*=e.w,this}multiplyScalar(e){return this.x*=e,this.y*=e,this.z*=e,this.w*=e,this}applyMatrix4(e){let t=this.x,n=this.y,r=this.z,i=this.w,a=e.elements;return this.x=a[0]*t+a[4]*n+a[8]*r+a[12]*i,this.y=a[1]*t+a[5]*n+a[9]*r+a[13]*i,this.z=a[2]*t+a[6]*n+a[10]*r+a[14]*i,this.w=a[3]*t+a[7]*n+a[11]*r+a[15]*i,this}divideScalar(e){return this.multiplyScalar(1/e)}setAxisAngleFromQuaternion(e){this.w=2*Math.acos(e.w);let t=Math.sqrt(1-e.w*e.w);return t<1e-4?(this.x=1,this.y=0,this.z=0):(this.x=e.x/t,this.y=e.y/t,this.z=e.z/t),this}setAxisAngleFromRotationMatrix(e){let t,n,r,i,a=.01,o=.1,s=e.elements,c=s[0],l=s[4],u=s[8],d=s[1],f=s[5],p=s[9],m=s[2],h=s[6],g=s[10];if(Math.abs(l-d)<a&&Math.abs(u-m)<a&&Math.abs(p-h)<a){if(Math.abs(l+d)<o&&Math.abs(u+m)<o&&Math.abs(p+h)<o&&Math.abs(c+f+g-3)<o)return this.set(1,0,0,0),this;t=Math.PI;let e=(c+1)/2,s=(f+1)/2,_=(g+1)/2,v=(l+d)/4,y=(u+m)/4,b=(p+h)/4;return e>s&&e>_?e<a?(n=0,r=.707106781,i=.707106781):(n=Math.sqrt(e),r=v/n,i=y/n):s>_?s<a?(n=.707106781,r=0,i=.707106781):(r=Math.sqrt(s),n=v/r,i=b/r):_<a?(n=.707106781,r=.707106781,i=0):(i=Math.sqrt(_),n=y/i,r=b/i),this.set(n,r,i,t),this}let _=Math.sqrt((h-p)*(h-p)+(u-m)*(u-m)+(d-l)*(d-l));return Math.abs(_)<.001&&(_=1),this.x=(h-p)/_,this.y=(u-m)/_,this.z=(d-l)/_,this.w=Math.acos((c+f+g-1)/2),this}min(e){return this.x=Math.min(this.x,e.x),this.y=Math.min(this.y,e.y),this.z=Math.min(this.z,e.z),this.w=Math.min(this.w,e.w),this}max(e){return this.x=Math.max(this.x,e.x),this.y=Math.max(this.y,e.y),this.z=Math.max(this.z,e.z),this.w=Math.max(this.w,e.w),this}clamp(e,t){return this.x=Math.max(e.x,Math.min(t.x,this.x)),this.y=Math.max(e.y,Math.min(t.y,this.y)),this.z=Math.max(e.z,Math.min(t.z,this.z)),this.w=Math.max(e.w,Math.min(t.w,this.w)),this}clampScalar(e,t){return this.x=Math.max(e,Math.min(t,this.x)),this.y=Math.max(e,Math.min(t,this.y)),this.z=Math.max(e,Math.min(t,this.z)),this.w=Math.max(e,Math.min(t,this.w)),this}clampLength(e,t){let n=this.length();return this.divideScalar(n||1).multiplyScalar(Math.max(e,Math.min(t,n)))}floor(){return this.x=Math.floor(this.x),this.y=Math.floor(this.y),this.z=Math.floor(this.z),this.w=Math.floor(this.w),this}ceil(){return this.x=Math.ceil(this.x),this.y=Math.ceil(this.y),this.z=Math.ceil(this.z),this.w=Math.ceil(this.w),this}round(){return this.x=Math.round(this.x),this.y=Math.round(this.y),this.z=Math.round(this.z),this.w=Math.round(this.w),this}roundToZero(){return this.x=Math.trunc(this.x),this.y=Math.trunc(this.y),this.z=Math.trunc(this.z),this.w=Math.trunc(this.w),this}negate(){return this.x=-this.x,this.y=-this.y,this.z=-this.z,this.w=-this.w,this}dot(e){return this.x*e.x+this.y*e.y+this.z*e.z+this.w*e.w}lengthSq(){return this.x*this.x+this.y*this.y+this.z*this.z+this.w*this.w}length(){return Math.sqrt(this.x*this.x+this.y*this.y+this.z*this.z+this.w*this.w)}manhattanLength(){return Math.abs(this.x)+Math.abs(this.y)+Math.abs(this.z)+Math.abs(this.w)}normalize(){return this.divideScalar(this.length()||1)}setLength(e){return this.normalize().multiplyScalar(e)}lerp(e,t){return this.x+=(e.x-this.x)*t,this.y+=(e.y-this.y)*t,this.z+=(e.z-this.z)*t,this.w+=(e.w-this.w)*t,this}lerpVectors(e,t,n){return this.x=e.x+(t.x-e.x)*n,this.y=e.y+(t.y-e.y)*n,this.z=e.z+(t.z-e.z)*n,this.w=e.w+(t.w-e.w)*n,this}equals(e){return e.x===this.x&&e.y===this.y&&e.z===this.z&&e.w===this.w}fromArray(e,t=0){return this.x=e[t],this.y=e[t+1],this.z=e[t+2],this.w=e[t+3],this}toArray(e=[],t=0){return e[t]=this.x,e[t+1]=this.y,e[t+2]=this.z,e[t+3]=this.w,e}fromBufferAttribute(e,t){return this.x=e.getX(t),this.y=e.getY(t),this.z=e.getZ(t),this.w=e.getW(t),this}random(){return this.x=Math.random(),this.y=Math.random(),this.z=Math.random(),this.w=Math.random(),this}*[Symbol.iterator](){yield this.x,yield this.y,yield this.z,yield this.w}},$r=class extends ir{constructor(e=1,t=1,n={}){super(),this.isRenderTarget=!0,this.width=e,this.height=t,this.depth=1,this.scissor=new Qr(0,0,e,t),this.scissorTest=!1,this.viewport=new Qr(0,0,e,t);let r={width:e,height:t,depth:1};n=Object.assign({generateMipmaps:!1,internalFormat:null,minFilter:On,depthBuffer:!0,stencilBuffer:!1,depthTexture:null,samples:0,count:1},n);let i=new Zr(r,n.mapping,n.wrapS,n.wrapT,n.magFilter,n.minFilter,n.format,n.type,n.anisotropy,n.colorSpace);i.flipY=!1,i.generateMipmaps=n.generateMipmaps,i.internalFormat=n.internalFormat,this.textures=[];let a=n.count;for(let e=0;e<a;e++)this.textures[e]=i.clone(),this.textures[e].isRenderTargetTexture=!0;this.depthBuffer=n.depthBuffer,this.stencilBuffer=n.stencilBuffer,this.depthTexture=n.depthTexture,this.samples=n.samples}get texture(){return this.textures[0]}set texture(e){this.textures[0]=e}setSize(e,t,n=1){if(this.width!==e||this.height!==t||this.depth!==n){this.width=e,this.height=t,this.depth=n;for(let r=0,i=this.textures.length;r<i;r++)this.textures[r].image.width=e,this.textures[r].image.height=t,this.textures[r].image.depth=n;this.dispose()}this.viewport.set(0,0,e,t),this.scissor.set(0,0,e,t)}clone(){return new this.constructor().copy(this)}copy(e){this.width=e.width,this.height=e.height,this.depth=e.depth,this.scissor.copy(e.scissor),this.scissorTest=e.scissorTest,this.viewport.copy(e.viewport),this.textures.length=0;for(let t=0,n=e.textures.length;t<n;t++)this.textures[t]=e.textures[t].clone(),this.textures[t].isRenderTargetTexture=!0;let t=Object.assign({},e.texture.image);return this.texture.source=new Jr(t),this.depthBuffer=e.depthBuffer,this.stencilBuffer=e.stencilBuffer,e.depthTexture!==null&&(this.depthTexture=e.depthTexture.clone()),this.samples=e.samples,this}dispose(){this.dispatchEvent({type:`dispose`})}},ei=class extends $r{constructor(e=1,t=1,n={}){super(e,t,n),this.isWebGLRenderTarget=!0}},ti=class extends Zr{constructor(e=null,t=1,n=1,r=1){super(null),this.isDataArrayTexture=!0,this.image={data:e,width:t,height:n,depth:r},this.magFilter=Tn,this.minFilter=Tn,this.wrapR=Cn,this.generateMipmaps=!1,this.flipY=!1,this.unpackAlignment=1}},ni=class extends Zr{constructor(e=null,t=1,n=1,r=1){super(null),this.isData3DTexture=!0,this.image={data:e,width:t,height:n,depth:r},this.magFilter=Tn,this.minFilter=Tn,this.wrapR=Cn,this.generateMipmaps=!1,this.flipY=!1,this.unpackAlignment=1}},ri=class{constructor(e=0,t=0,n=0,r=1){this.isQuaternion=!0,this._x=e,this._y=t,this._z=n,this._w=r}static slerpFlat(e,t,n,r,i,a,o){let s=n[r+0],c=n[r+1],l=n[r+2],u=n[r+3],d=i[a+0],f=i[a+1],p=i[a+2],m=i[a+3];if(o===0){e[t+0]=s,e[t+1]=c,e[t+2]=l,e[t+3]=u;return}if(o===1){e[t+0]=d,e[t+1]=f,e[t+2]=p,e[t+3]=m;return}if(u!==m||s!==d||c!==f||l!==p){let e=1-o,t=s*d+c*f+l*p+u*m,n=t>=0?1:-1,r=1-t*t;if(r>2**-52){let i=Math.sqrt(r),a=Math.atan2(i,t*n);e=Math.sin(e*a)/i,o=Math.sin(o*a)/i}let i=o*n;if(s=s*e+d*i,c=c*e+f*i,l=l*e+p*i,u=u*e+m*i,e===1-o){let e=1/Math.sqrt(s*s+c*c+l*l+u*u);s*=e,c*=e,l*=e,u*=e}}e[t]=s,e[t+1]=c,e[t+2]=l,e[t+3]=u}static multiplyQuaternionsFlat(e,t,n,r,i,a){let o=n[r],s=n[r+1],c=n[r+2],l=n[r+3],u=i[a],d=i[a+1],f=i[a+2],p=i[a+3];return e[t]=o*p+l*u+s*f-c*d,e[t+1]=s*p+l*d+c*u-o*f,e[t+2]=c*p+l*f+o*d-s*u,e[t+3]=l*p-o*u-s*d-c*f,e}get x(){return this._x}set x(e){this._x=e,this._onChangeCallback()}get y(){return this._y}set y(e){this._y=e,this._onChangeCallback()}get z(){return this._z}set z(e){this._z=e,this._onChangeCallback()}get w(){return this._w}set w(e){this._w=e,this._onChangeCallback()}set(e,t,n,r){return this._x=e,this._y=t,this._z=n,this._w=r,this._onChangeCallback(),this}clone(){return new this.constructor(this._x,this._y,this._z,this._w)}copy(e){return this._x=e.x,this._y=e.y,this._z=e.z,this._w=e.w,this._onChangeCallback(),this}setFromEuler(e,t=!0){let n=e._x,r=e._y,i=e._z,a=e._order,o=Math.cos,s=Math.sin,c=o(n/2),l=o(r/2),u=o(i/2),d=s(n/2),f=s(r/2),p=s(i/2);switch(a){case`XYZ`:this._x=d*l*u+c*f*p,this._y=c*f*u-d*l*p,this._z=c*l*p+d*f*u,this._w=c*l*u-d*f*p;break;case`YXZ`:this._x=d*l*u+c*f*p,this._y=c*f*u-d*l*p,this._z=c*l*p-d*f*u,this._w=c*l*u+d*f*p;break;case`ZXY`:this._x=d*l*u-c*f*p,this._y=c*f*u+d*l*p,this._z=c*l*p+d*f*u,this._w=c*l*u-d*f*p;break;case`ZYX`:this._x=d*l*u-c*f*p,this._y=c*f*u+d*l*p,this._z=c*l*p-d*f*u,this._w=c*l*u+d*f*p;break;case`YZX`:this._x=d*l*u+c*f*p,this._y=c*f*u+d*l*p,this._z=c*l*p-d*f*u,this._w=c*l*u-d*f*p;break;case`XZY`:this._x=d*l*u-c*f*p,this._y=c*f*u-d*l*p,this._z=c*l*p+d*f*u,this._w=c*l*u+d*f*p;break;default:console.warn(`THREE.Quaternion: .setFromEuler() encountered an unknown order: `+a)}return t===!0&&this._onChangeCallback(),this}setFromAxisAngle(e,t){let n=t/2,r=Math.sin(n);return this._x=e.x*r,this._y=e.y*r,this._z=e.z*r,this._w=Math.cos(n),this._onChangeCallback(),this}setFromRotationMatrix(e){let t=e.elements,n=t[0],r=t[4],i=t[8],a=t[1],o=t[5],s=t[9],c=t[2],l=t[6],u=t[10],d=n+o+u;if(d>0){let e=.5/Math.sqrt(d+1);this._w=.25/e,this._x=(l-s)*e,this._y=(i-c)*e,this._z=(a-r)*e}else if(n>o&&n>u){let e=2*Math.sqrt(1+n-o-u);this._w=(l-s)/e,this._x=.25*e,this._y=(r+a)/e,this._z=(i+c)/e}else if(o>u){let e=2*Math.sqrt(1+o-n-u);this._w=(i-c)/e,this._x=(r+a)/e,this._y=.25*e,this._z=(s+l)/e}else{let e=2*Math.sqrt(1+u-n-o);this._w=(a-r)/e,this._x=(i+c)/e,this._y=(s+l)/e,this._z=.25*e}return this._onChangeCallback(),this}setFromUnitVectors(e,t){let n=e.dot(t)+1;return n<2**-52?(n=0,Math.abs(e.x)>Math.abs(e.z)?(this._x=-e.y,this._y=e.x,this._z=0,this._w=n):(this._x=0,this._y=-e.z,this._z=e.y,this._w=n)):(this._x=e.y*t.z-e.z*t.y,this._y=e.z*t.x-e.x*t.z,this._z=e.x*t.y-e.y*t.x,this._w=n),this.normalize()}angleTo(e){return 2*Math.acos(Math.abs(ur(this.dot(e),-1,1)))}rotateTowards(e,t){let n=this.angleTo(e);if(n===0)return this;let r=Math.min(1,t/n);return this.slerp(e,r),this}identity(){return this.set(0,0,0,1)}invert(){return this.conjugate()}conjugate(){return this._x*=-1,this._y*=-1,this._z*=-1,this._onChangeCallback(),this}dot(e){return this._x*e._x+this._y*e._y+this._z*e._z+this._w*e._w}lengthSq(){return this._x*this._x+this._y*this._y+this._z*this._z+this._w*this._w}length(){return Math.sqrt(this._x*this._x+this._y*this._y+this._z*this._z+this._w*this._w)}normalize(){let e=this.length();return e===0?(this._x=0,this._y=0,this._z=0,this._w=1):(e=1/e,this._x*=e,this._y*=e,this._z*=e,this._w*=e),this._onChangeCallback(),this}multiply(e){return this.multiplyQuaternions(this,e)}premultiply(e){return this.multiplyQuaternions(e,this)}multiplyQuaternions(e,t){let n=e._x,r=e._y,i=e._z,a=e._w,o=t._x,s=t._y,c=t._z,l=t._w;return this._x=n*l+a*o+r*c-i*s,this._y=r*l+a*s+i*o-n*c,this._z=i*l+a*c+n*s-r*o,this._w=a*l-n*o-r*s-i*c,this._onChangeCallback(),this}slerp(e,t){if(t===0)return this;if(t===1)return this.copy(e);let n=this._x,r=this._y,i=this._z,a=this._w,o=a*e._w+n*e._x+r*e._y+i*e._z;if(o<0?(this._w=-e._w,this._x=-e._x,this._y=-e._y,this._z=-e._z,o=-o):this.copy(e),o>=1)return this._w=a,this._x=n,this._y=r,this._z=i,this;let s=1-o*o;if(s<=2**-52){let e=1-t;return this._w=e*a+t*this._w,this._x=e*n+t*this._x,this._y=e*r+t*this._y,this._z=e*i+t*this._z,this.normalize(),this}let c=Math.sqrt(s),l=Math.atan2(c,o),u=Math.sin((1-t)*l)/c,d=Math.sin(t*l)/c;return this._w=a*u+this._w*d,this._x=n*u+this._x*d,this._y=r*u+this._y*d,this._z=i*u+this._z*d,this._onChangeCallback(),this}slerpQuaternions(e,t,n){return this.copy(e).slerp(t,n)}random(){let e=2*Math.PI*Math.random(),t=2*Math.PI*Math.random(),n=Math.random(),r=Math.sqrt(1-n),i=Math.sqrt(n);return this.set(r*Math.sin(e),r*Math.cos(e),i*Math.sin(t),i*Math.cos(t))}equals(e){return e._x===this._x&&e._y===this._y&&e._z===this._z&&e._w===this._w}fromArray(e,t=0){return this._x=e[t],this._y=e[t+1],this._z=e[t+2],this._w=e[t+3],this._onChangeCallback(),this}toArray(e=[],t=0){return e[t]=this._x,e[t+1]=this._y,e[t+2]=this._z,e[t+3]=this._w,e}fromBufferAttribute(e,t){return this._x=e.getX(t),this._y=e.getY(t),this._z=e.getZ(t),this._w=e.getW(t),this._onChangeCallback(),this}toJSON(){return this.toArray()}_onChange(e){return this._onChangeCallback=e,this}_onChangeCallback(){}*[Symbol.iterator](){yield this._x,yield this._y,yield this._z,yield this._w}},Z=class e{constructor(t=0,n=0,r=0){e.prototype.isVector3=!0,this.x=t,this.y=n,this.z=r}set(e,t,n){return n===void 0&&(n=this.z),this.x=e,this.y=t,this.z=n,this}setScalar(e){return this.x=e,this.y=e,this.z=e,this}setX(e){return this.x=e,this}setY(e){return this.y=e,this}setZ(e){return this.z=e,this}setComponent(e,t){switch(e){case 0:this.x=t;break;case 1:this.y=t;break;case 2:this.z=t;break;default:throw Error(`index is out of range: `+e)}return this}getComponent(e){switch(e){case 0:return this.x;case 1:return this.y;case 2:return this.z;default:throw Error(`index is out of range: `+e)}}clone(){return new this.constructor(this.x,this.y,this.z)}copy(e){return this.x=e.x,this.y=e.y,this.z=e.z,this}add(e){return this.x+=e.x,this.y+=e.y,this.z+=e.z,this}addScalar(e){return this.x+=e,this.y+=e,this.z+=e,this}addVectors(e,t){return this.x=e.x+t.x,this.y=e.y+t.y,this.z=e.z+t.z,this}addScaledVector(e,t){return this.x+=e.x*t,this.y+=e.y*t,this.z+=e.z*t,this}sub(e){return this.x-=e.x,this.y-=e.y,this.z-=e.z,this}subScalar(e){return this.x-=e,this.y-=e,this.z-=e,this}subVectors(e,t){return this.x=e.x-t.x,this.y=e.y-t.y,this.z=e.z-t.z,this}multiply(e){return this.x*=e.x,this.y*=e.y,this.z*=e.z,this}multiplyScalar(e){return this.x*=e,this.y*=e,this.z*=e,this}multiplyVectors(e,t){return this.x=e.x*t.x,this.y=e.y*t.y,this.z=e.z*t.z,this}applyEuler(e){return this.applyQuaternion(ai.setFromEuler(e))}applyAxisAngle(e,t){return this.applyQuaternion(ai.setFromAxisAngle(e,t))}applyMatrix3(e){let t=this.x,n=this.y,r=this.z,i=e.elements;return this.x=i[0]*t+i[3]*n+i[6]*r,this.y=i[1]*t+i[4]*n+i[7]*r,this.z=i[2]*t+i[5]*n+i[8]*r,this}applyNormalMatrix(e){return this.applyMatrix3(e).normalize()}applyMatrix4(e){let t=this.x,n=this.y,r=this.z,i=e.elements,a=1/(i[3]*t+i[7]*n+i[11]*r+i[15]);return this.x=(i[0]*t+i[4]*n+i[8]*r+i[12])*a,this.y=(i[1]*t+i[5]*n+i[9]*r+i[13])*a,this.z=(i[2]*t+i[6]*n+i[10]*r+i[14])*a,this}applyQuaternion(e){let t=this.x,n=this.y,r=this.z,i=e.x,a=e.y,o=e.z,s=e.w,c=2*(a*r-o*n),l=2*(o*t-i*r),u=2*(i*n-a*t);return this.x=t+s*c+a*u-o*l,this.y=n+s*l+o*c-i*u,this.z=r+s*u+i*l-a*c,this}project(e){return this.applyMatrix4(e.matrixWorldInverse).applyMatrix4(e.projectionMatrix)}unproject(e){return this.applyMatrix4(e.projectionMatrixInverse).applyMatrix4(e.matrixWorld)}transformDirection(e){let t=this.x,n=this.y,r=this.z,i=e.elements;return this.x=i[0]*t+i[4]*n+i[8]*r,this.y=i[1]*t+i[5]*n+i[9]*r,this.z=i[2]*t+i[6]*n+i[10]*r,this.normalize()}divide(e){return this.x/=e.x,this.y/=e.y,this.z/=e.z,this}divideScalar(e){return this.multiplyScalar(1/e)}min(e){return this.x=Math.min(this.x,e.x),this.y=Math.min(this.y,e.y),this.z=Math.min(this.z,e.z),this}max(e){return this.x=Math.max(this.x,e.x),this.y=Math.max(this.y,e.y),this.z=Math.max(this.z,e.z),this}clamp(e,t){return this.x=Math.max(e.x,Math.min(t.x,this.x)),this.y=Math.max(e.y,Math.min(t.y,this.y)),this.z=Math.max(e.z,Math.min(t.z,this.z)),this}clampScalar(e,t){return this.x=Math.max(e,Math.min(t,this.x)),this.y=Math.max(e,Math.min(t,this.y)),this.z=Math.max(e,Math.min(t,this.z)),this}clampLength(e,t){let n=this.length();return this.divideScalar(n||1).multiplyScalar(Math.max(e,Math.min(t,n)))}floor(){return this.x=Math.floor(this.x),this.y=Math.floor(this.y),this.z=Math.floor(this.z),this}ceil(){return this.x=Math.ceil(this.x),this.y=Math.ceil(this.y),this.z=Math.ceil(this.z),this}round(){return this.x=Math.round(this.x),this.y=Math.round(this.y),this.z=Math.round(this.z),this}roundToZero(){return this.x=Math.trunc(this.x),this.y=Math.trunc(this.y),this.z=Math.trunc(this.z),this}negate(){return this.x=-this.x,this.y=-this.y,this.z=-this.z,this}dot(e){return this.x*e.x+this.y*e.y+this.z*e.z}lengthSq(){return this.x*this.x+this.y*this.y+this.z*this.z}length(){return Math.sqrt(this.x*this.x+this.y*this.y+this.z*this.z)}manhattanLength(){return Math.abs(this.x)+Math.abs(this.y)+Math.abs(this.z)}normalize(){return this.divideScalar(this.length()||1)}setLength(e){return this.normalize().multiplyScalar(e)}lerp(e,t){return this.x+=(e.x-this.x)*t,this.y+=(e.y-this.y)*t,this.z+=(e.z-this.z)*t,this}lerpVectors(e,t,n){return this.x=e.x+(t.x-e.x)*n,this.y=e.y+(t.y-e.y)*n,this.z=e.z+(t.z-e.z)*n,this}cross(e){return this.crossVectors(this,e)}crossVectors(e,t){let n=e.x,r=e.y,i=e.z,a=t.x,o=t.y,s=t.z;return this.x=r*s-i*o,this.y=i*a-n*s,this.z=n*o-r*a,this}projectOnVector(e){let t=e.lengthSq();if(t===0)return this.set(0,0,0);let n=e.dot(this)/t;return this.copy(e).multiplyScalar(n)}projectOnPlane(e){return ii.copy(this).projectOnVector(e),this.sub(ii)}reflect(e){return this.sub(ii.copy(e).multiplyScalar(2*this.dot(e)))}angleTo(e){let t=Math.sqrt(this.lengthSq()*e.lengthSq());if(t===0)return Math.PI/2;let n=this.dot(e)/t;return Math.acos(ur(n,-1,1))}distanceTo(e){return Math.sqrt(this.distanceToSquared(e))}distanceToSquared(e){let t=this.x-e.x,n=this.y-e.y,r=this.z-e.z;return t*t+n*n+r*r}manhattanDistanceTo(e){return Math.abs(this.x-e.x)+Math.abs(this.y-e.y)+Math.abs(this.z-e.z)}setFromSpherical(e){return this.setFromSphericalCoords(e.radius,e.phi,e.theta)}setFromSphericalCoords(e,t,n){let r=Math.sin(t)*e;return this.x=r*Math.sin(n),this.y=Math.cos(t)*e,this.z=r*Math.cos(n),this}setFromCylindrical(e){return this.setFromCylindricalCoords(e.radius,e.theta,e.y)}setFromCylindricalCoords(e,t,n){return this.x=e*Math.sin(t),this.y=n,this.z=e*Math.cos(t),this}setFromMatrixPosition(e){let t=e.elements;return this.x=t[12],this.y=t[13],this.z=t[14],this}setFromMatrixScale(e){let t=this.setFromMatrixColumn(e,0).length(),n=this.setFromMatrixColumn(e,1).length(),r=this.setFromMatrixColumn(e,2).length();return this.x=t,this.y=n,this.z=r,this}setFromMatrixColumn(e,t){return this.fromArray(e.elements,t*4)}setFromMatrix3Column(e,t){return this.fromArray(e.elements,t*3)}setFromEuler(e){return this.x=e._x,this.y=e._y,this.z=e._z,this}setFromColor(e){return this.x=e.r,this.y=e.g,this.z=e.b,this}equals(e){return e.x===this.x&&e.y===this.y&&e.z===this.z}fromArray(e,t=0){return this.x=e[t],this.y=e[t+1],this.z=e[t+2],this}toArray(e=[],t=0){return e[t]=this.x,e[t+1]=this.y,e[t+2]=this.z,e}fromBufferAttribute(e,t){return this.x=e.getX(t),this.y=e.getY(t),this.z=e.getZ(t),this}random(){return this.x=Math.random(),this.y=Math.random(),this.z=Math.random(),this}randomDirection(){let e=Math.random()*Math.PI*2,t=Math.random()*2-1,n=Math.sqrt(1-t*t);return this.x=n*Math.cos(e),this.y=t,this.z=n*Math.sin(e),this}*[Symbol.iterator](){yield this.x,yield this.y,yield this.z}},ii=new Z,ai=new ri,oi=class{constructor(e=new Z(1/0,1/0,1/0),t=new Z(-1/0,-1/0,-1/0)){this.isBox3=!0,this.min=e,this.max=t}set(e,t){return this.min.copy(e),this.max.copy(t),this}setFromArray(e){this.makeEmpty();for(let t=0,n=e.length;t<n;t+=3)this.expandByPoint(ci.fromArray(e,t));return this}setFromBufferAttribute(e){this.makeEmpty();for(let t=0,n=e.count;t<n;t++)this.expandByPoint(ci.fromBufferAttribute(e,t));return this}setFromPoints(e){this.makeEmpty();for(let t=0,n=e.length;t<n;t++)this.expandByPoint(e[t]);return this}setFromCenterAndSize(e,t){let n=ci.copy(t).multiplyScalar(.5);return this.min.copy(e).sub(n),this.max.copy(e).add(n),this}setFromObject(e,t=!1){return this.makeEmpty(),this.expandByObject(e,t)}clone(){return new this.constructor().copy(this)}copy(e){return this.min.copy(e.min),this.max.copy(e.max),this}makeEmpty(){return this.min.x=this.min.y=this.min.z=1/0,this.max.x=this.max.y=this.max.z=-1/0,this}isEmpty(){return this.max.x<this.min.x||this.max.y<this.min.y||this.max.z<this.min.z}getCenter(e){return this.isEmpty()?e.set(0,0,0):e.addVectors(this.min,this.max).multiplyScalar(.5)}getSize(e){return this.isEmpty()?e.set(0,0,0):e.subVectors(this.max,this.min)}expandByPoint(e){return this.min.min(e),this.max.max(e),this}expandByVector(e){return this.min.sub(e),this.max.add(e),this}expandByScalar(e){return this.min.addScalar(-e),this.max.addScalar(e),this}expandByObject(e,t=!1){e.updateWorldMatrix(!1,!1);let n=e.geometry;if(n!==void 0){let r=n.getAttribute(`position`);if(t===!0&&r!==void 0&&e.isInstancedMesh!==!0)for(let t=0,n=r.count;t<n;t++)e.isMesh===!0?e.getVertexPosition(t,ci):ci.fromBufferAttribute(r,t),ci.applyMatrix4(e.matrixWorld),this.expandByPoint(ci);else e.boundingBox===void 0?(n.boundingBox===null&&n.computeBoundingBox(),li.copy(n.boundingBox)):(e.boundingBox===null&&e.computeBoundingBox(),li.copy(e.boundingBox)),li.applyMatrix4(e.matrixWorld),this.union(li)}let r=e.children;for(let e=0,n=r.length;e<n;e++)this.expandByObject(r[e],t);return this}containsPoint(e){return!(e.x<this.min.x||e.x>this.max.x||e.y<this.min.y||e.y>this.max.y||e.z<this.min.z||e.z>this.max.z)}containsBox(e){return this.min.x<=e.min.x&&e.max.x<=this.max.x&&this.min.y<=e.min.y&&e.max.y<=this.max.y&&this.min.z<=e.min.z&&e.max.z<=this.max.z}getParameter(e,t){return t.set((e.x-this.min.x)/(this.max.x-this.min.x),(e.y-this.min.y)/(this.max.y-this.min.y),(e.z-this.min.z)/(this.max.z-this.min.z))}intersectsBox(e){return!(e.max.x<this.min.x||e.min.x>this.max.x||e.max.y<this.min.y||e.min.y>this.max.y||e.max.z<this.min.z||e.min.z>this.max.z)}intersectsSphere(e){return this.clampPoint(e.center,ci),ci.distanceToSquared(e.center)<=e.radius*e.radius}intersectsPlane(e){let t,n;return e.normal.x>0?(t=e.normal.x*this.min.x,n=e.normal.x*this.max.x):(t=e.normal.x*this.max.x,n=e.normal.x*this.min.x),e.normal.y>0?(t+=e.normal.y*this.min.y,n+=e.normal.y*this.max.y):(t+=e.normal.y*this.max.y,n+=e.normal.y*this.min.y),e.normal.z>0?(t+=e.normal.z*this.min.z,n+=e.normal.z*this.max.z):(t+=e.normal.z*this.max.z,n+=e.normal.z*this.min.z),t<=-e.constant&&n>=-e.constant}intersectsTriangle(e){if(this.isEmpty())return!1;this.getCenter(gi),_i.subVectors(this.max,gi),ui.subVectors(e.a,gi),di.subVectors(e.b,gi),fi.subVectors(e.c,gi),pi.subVectors(di,ui),mi.subVectors(fi,di),hi.subVectors(ui,fi);let t=[0,-pi.z,pi.y,0,-mi.z,mi.y,0,-hi.z,hi.y,pi.z,0,-pi.x,mi.z,0,-mi.x,hi.z,0,-hi.x,-pi.y,pi.x,0,-mi.y,mi.x,0,-hi.y,hi.x,0];return!bi(t,ui,di,fi,_i)||(t=[1,0,0,0,1,0,0,0,1],!bi(t,ui,di,fi,_i))?!1:(vi.crossVectors(pi,mi),t=[vi.x,vi.y,vi.z],bi(t,ui,di,fi,_i))}clampPoint(e,t){return t.copy(e).clamp(this.min,this.max)}distanceToPoint(e){return this.clampPoint(e,ci).distanceTo(e)}getBoundingSphere(e){return this.isEmpty()?e.makeEmpty():(this.getCenter(e.center),e.radius=this.getSize(ci).length()*.5),e}intersect(e){return this.min.max(e.min),this.max.min(e.max),this.isEmpty()&&this.makeEmpty(),this}union(e){return this.min.min(e.min),this.max.max(e.max),this}applyMatrix4(e){return this.isEmpty()?this:(si[0].set(this.min.x,this.min.y,this.min.z).applyMatrix4(e),si[1].set(this.min.x,this.min.y,this.max.z).applyMatrix4(e),si[2].set(this.min.x,this.max.y,this.min.z).applyMatrix4(e),si[3].set(this.min.x,this.max.y,this.max.z).applyMatrix4(e),si[4].set(this.max.x,this.min.y,this.min.z).applyMatrix4(e),si[5].set(this.max.x,this.min.y,this.max.z).applyMatrix4(e),si[6].set(this.max.x,this.max.y,this.min.z).applyMatrix4(e),si[7].set(this.max.x,this.max.y,this.max.z).applyMatrix4(e),this.setFromPoints(si),this)}translate(e){return this.min.add(e),this.max.add(e),this}equals(e){return e.min.equals(this.min)&&e.max.equals(this.max)}},si=[new Z,new Z,new Z,new Z,new Z,new Z,new Z,new Z],ci=new Z,li=new oi,ui=new Z,di=new Z,fi=new Z,pi=new Z,mi=new Z,hi=new Z,gi=new Z,_i=new Z,vi=new Z,yi=new Z;function bi(e,t,n,r,i){for(let a=0,o=e.length-3;a<=o;a+=3){yi.fromArray(e,a);let o=i.x*Math.abs(yi.x)+i.y*Math.abs(yi.y)+i.z*Math.abs(yi.z),s=t.dot(yi),c=n.dot(yi),l=r.dot(yi);if(Math.max(-Math.max(s,c,l),Math.min(s,c,l))>o)return!1}return!0}var xi=new oi,Si=new Z,Ci=new Z,wi=class{constructor(e=new Z,t=-1){this.isSphere=!0,this.center=e,this.radius=t}set(e,t){return this.center.copy(e),this.radius=t,this}setFromPoints(e,t){let n=this.center;t===void 0?xi.setFromPoints(e).getCenter(n):n.copy(t);let r=0;for(let t=0,i=e.length;t<i;t++)r=Math.max(r,n.distanceToSquared(e[t]));return this.radius=Math.sqrt(r),this}copy(e){return this.center.copy(e.center),this.radius=e.radius,this}isEmpty(){return this.radius<0}makeEmpty(){return this.center.set(0,0,0),this.radius=-1,this}containsPoint(e){return e.distanceToSquared(this.center)<=this.radius*this.radius}distanceToPoint(e){return e.distanceTo(this.center)-this.radius}intersectsSphere(e){let t=this.radius+e.radius;return e.center.distanceToSquared(this.center)<=t*t}intersectsBox(e){return e.intersectsSphere(this)}intersectsPlane(e){return Math.abs(e.distanceToPoint(this.center))<=this.radius}clampPoint(e,t){let n=this.center.distanceToSquared(e);return t.copy(e),n>this.radius*this.radius&&(t.sub(this.center).normalize(),t.multiplyScalar(this.radius).add(this.center)),t}getBoundingBox(e){return this.isEmpty()?(e.makeEmpty(),e):(e.set(this.center,this.center),e.expandByScalar(this.radius),e)}applyMatrix4(e){return this.center.applyMatrix4(e),this.radius*=e.getMaxScaleOnAxis(),this}translate(e){return this.center.add(e),this}expandByPoint(e){if(this.isEmpty())return this.center.copy(e),this.radius=0,this;Si.subVectors(e,this.center);let t=Si.lengthSq();if(t>this.radius*this.radius){let e=Math.sqrt(t),n=(e-this.radius)*.5;this.center.addScaledVector(Si,n/e),this.radius+=n}return this}union(e){return e.isEmpty()?this:this.isEmpty()?(this.copy(e),this):(this.center.equals(e.center)===!0?this.radius=Math.max(this.radius,e.radius):(Ci.subVectors(e.center,this.center).setLength(e.radius),this.expandByPoint(Si.copy(e.center).add(Ci)),this.expandByPoint(Si.copy(e.center).sub(Ci))),this)}equals(e){return e.center.equals(this.center)&&e.radius===this.radius}clone(){return new this.constructor().copy(this)}},Ti=new Z,Ei=new Z,Di=new Z,Oi=new Z,ki=new Z,Ai=new Z,ji=new Z,Mi=class{constructor(e=new Z,t=new Z(0,0,-1)){this.origin=e,this.direction=t}set(e,t){return this.origin.copy(e),this.direction.copy(t),this}copy(e){return this.origin.copy(e.origin),this.direction.copy(e.direction),this}at(e,t){return t.copy(this.origin).addScaledVector(this.direction,e)}lookAt(e){return this.direction.copy(e).sub(this.origin).normalize(),this}recast(e){return this.origin.copy(this.at(e,Ti)),this}closestPointToPoint(e,t){t.subVectors(e,this.origin);let n=t.dot(this.direction);return n<0?t.copy(this.origin):t.copy(this.origin).addScaledVector(this.direction,n)}distanceToPoint(e){return Math.sqrt(this.distanceSqToPoint(e))}distanceSqToPoint(e){let t=Ti.subVectors(e,this.origin).dot(this.direction);return t<0?this.origin.distanceToSquared(e):(Ti.copy(this.origin).addScaledVector(this.direction,t),Ti.distanceToSquared(e))}distanceSqToSegment(e,t,n,r){Ei.copy(e).add(t).multiplyScalar(.5),Di.copy(t).sub(e).normalize(),Oi.copy(this.origin).sub(Ei);let i=e.distanceTo(t)*.5,a=-this.direction.dot(Di),o=Oi.dot(this.direction),s=-Oi.dot(Di),c=Oi.lengthSq(),l=Math.abs(1-a*a),u,d,f,p;if(l>0)if(u=a*s-o,d=a*o-s,p=i*l,u>=0)if(d>=-p)if(d<=p){let e=1/l;u*=e,d*=e,f=u*(u+a*d+2*o)+d*(a*u+d+2*s)+c}else d=i,u=Math.max(0,-(a*d+o)),f=-u*u+d*(d+2*s)+c;else d=-i,u=Math.max(0,-(a*d+o)),f=-u*u+d*(d+2*s)+c;else d<=-p?(u=Math.max(0,-(-a*i+o)),d=u>0?-i:Math.min(Math.max(-i,-s),i),f=-u*u+d*(d+2*s)+c):d<=p?(u=0,d=Math.min(Math.max(-i,-s),i),f=d*(d+2*s)+c):(u=Math.max(0,-(a*i+o)),d=u>0?i:Math.min(Math.max(-i,-s),i),f=-u*u+d*(d+2*s)+c);else d=a>0?-i:i,u=Math.max(0,-(a*d+o)),f=-u*u+d*(d+2*s)+c;return n&&n.copy(this.origin).addScaledVector(this.direction,u),r&&r.copy(Ei).addScaledVector(Di,d),f}intersectSphere(e,t){Ti.subVectors(e.center,this.origin);let n=Ti.dot(this.direction),r=Ti.dot(Ti)-n*n,i=e.radius*e.radius;if(r>i)return null;let a=Math.sqrt(i-r),o=n-a,s=n+a;return s<0?null:o<0?this.at(s,t):this.at(o,t)}intersectsSphere(e){return this.distanceSqToPoint(e.center)<=e.radius*e.radius}distanceToPlane(e){let t=e.normal.dot(this.direction);if(t===0)return e.distanceToPoint(this.origin)===0?0:null;let n=-(this.origin.dot(e.normal)+e.constant)/t;return n>=0?n:null}intersectPlane(e,t){let n=this.distanceToPlane(e);return n===null?null:this.at(n,t)}intersectsPlane(e){let t=e.distanceToPoint(this.origin);return t===0||e.normal.dot(this.direction)*t<0}intersectBox(e,t){let n,r,i,a,o,s,c=1/this.direction.x,l=1/this.direction.y,u=1/this.direction.z,d=this.origin;return c>=0?(n=(e.min.x-d.x)*c,r=(e.max.x-d.x)*c):(n=(e.max.x-d.x)*c,r=(e.min.x-d.x)*c),l>=0?(i=(e.min.y-d.y)*l,a=(e.max.y-d.y)*l):(i=(e.max.y-d.y)*l,a=(e.min.y-d.y)*l),n>a||i>r||((i>n||isNaN(n))&&(n=i),(a<r||isNaN(r))&&(r=a),u>=0?(o=(e.min.z-d.z)*u,s=(e.max.z-d.z)*u):(o=(e.max.z-d.z)*u,s=(e.min.z-d.z)*u),n>s||o>r)||((o>n||n!==n)&&(n=o),(s<r||r!==r)&&(r=s),r<0)?null:this.at(n>=0?n:r,t)}intersectsBox(e){return this.intersectBox(e,Ti)!==null}intersectTriangle(e,t,n,r,i){ki.subVectors(t,e),Ai.subVectors(n,e),ji.crossVectors(ki,Ai);let a=this.direction.dot(ji),o;if(a>0){if(r)return null;o=1}else if(a<0)o=-1,a=-a;else return null;Oi.subVectors(this.origin,e);let s=o*this.direction.dot(Ai.crossVectors(Oi,Ai));if(s<0)return null;let c=o*this.direction.dot(ki.cross(Oi));if(c<0||s+c>a)return null;let l=-o*Oi.dot(ji);return l<0?null:this.at(l/a,i)}applyMatrix4(e){return this.origin.applyMatrix4(e),this.direction.transformDirection(e),this}equals(e){return e.origin.equals(this.origin)&&e.direction.equals(this.direction)}clone(){return new this.constructor().copy(this)}},Ni=class e{constructor(t,n,r,i,a,o,s,c,l,u,d,f,p,m,h,g){e.prototype.isMatrix4=!0,this.elements=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],t!==void 0&&this.set(t,n,r,i,a,o,s,c,l,u,d,f,p,m,h,g)}set(e,t,n,r,i,a,o,s,c,l,u,d,f,p,m,h){let g=this.elements;return g[0]=e,g[4]=t,g[8]=n,g[12]=r,g[1]=i,g[5]=a,g[9]=o,g[13]=s,g[2]=c,g[6]=l,g[10]=u,g[14]=d,g[3]=f,g[7]=p,g[11]=m,g[15]=h,this}identity(){return this.set(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1),this}clone(){return new e().fromArray(this.elements)}copy(e){let t=this.elements,n=e.elements;return t[0]=n[0],t[1]=n[1],t[2]=n[2],t[3]=n[3],t[4]=n[4],t[5]=n[5],t[6]=n[6],t[7]=n[7],t[8]=n[8],t[9]=n[9],t[10]=n[10],t[11]=n[11],t[12]=n[12],t[13]=n[13],t[14]=n[14],t[15]=n[15],this}copyPosition(e){let t=this.elements,n=e.elements;return t[12]=n[12],t[13]=n[13],t[14]=n[14],this}setFromMatrix3(e){let t=e.elements;return this.set(t[0],t[3],t[6],0,t[1],t[4],t[7],0,t[2],t[5],t[8],0,0,0,0,1),this}extractBasis(e,t,n){return e.setFromMatrixColumn(this,0),t.setFromMatrixColumn(this,1),n.setFromMatrixColumn(this,2),this}makeBasis(e,t,n){return this.set(e.x,t.x,n.x,0,e.y,t.y,n.y,0,e.z,t.z,n.z,0,0,0,0,1),this}extractRotation(e){let t=this.elements,n=e.elements,r=1/Pi.setFromMatrixColumn(e,0).length(),i=1/Pi.setFromMatrixColumn(e,1).length(),a=1/Pi.setFromMatrixColumn(e,2).length();return t[0]=n[0]*r,t[1]=n[1]*r,t[2]=n[2]*r,t[3]=0,t[4]=n[4]*i,t[5]=n[5]*i,t[6]=n[6]*i,t[7]=0,t[8]=n[8]*a,t[9]=n[9]*a,t[10]=n[10]*a,t[11]=0,t[12]=0,t[13]=0,t[14]=0,t[15]=1,this}makeRotationFromEuler(e){let t=this.elements,n=e.x,r=e.y,i=e.z,a=Math.cos(n),o=Math.sin(n),s=Math.cos(r),c=Math.sin(r),l=Math.cos(i),u=Math.sin(i);if(e.order===`XYZ`){let e=a*l,n=a*u,r=o*l,i=o*u;t[0]=s*l,t[4]=-s*u,t[8]=c,t[1]=n+r*c,t[5]=e-i*c,t[9]=-o*s,t[2]=i-e*c,t[6]=r+n*c,t[10]=a*s}else if(e.order===`YXZ`){let e=s*l,n=s*u,r=c*l,i=c*u;t[0]=e+i*o,t[4]=r*o-n,t[8]=a*c,t[1]=a*u,t[5]=a*l,t[9]=-o,t[2]=n*o-r,t[6]=i+e*o,t[10]=a*s}else if(e.order===`ZXY`){let e=s*l,n=s*u,r=c*l,i=c*u;t[0]=e-i*o,t[4]=-a*u,t[8]=r+n*o,t[1]=n+r*o,t[5]=a*l,t[9]=i-e*o,t[2]=-a*c,t[6]=o,t[10]=a*s}else if(e.order===`ZYX`){let e=a*l,n=a*u,r=o*l,i=o*u;t[0]=s*l,t[4]=r*c-n,t[8]=e*c+i,t[1]=s*u,t[5]=i*c+e,t[9]=n*c-r,t[2]=-c,t[6]=o*s,t[10]=a*s}else if(e.order===`YZX`){let e=a*s,n=a*c,r=o*s,i=o*c;t[0]=s*l,t[4]=i-e*u,t[8]=r*u+n,t[1]=u,t[5]=a*l,t[9]=-o*l,t[2]=-c*l,t[6]=n*u+r,t[10]=e-i*u}else if(e.order===`XZY`){let e=a*s,n=a*c,r=o*s,i=o*c;t[0]=s*l,t[4]=-u,t[8]=c*l,t[1]=e*u+i,t[5]=a*l,t[9]=n*u-r,t[2]=r*u-n,t[6]=o*l,t[10]=i*u+e}return t[3]=0,t[7]=0,t[11]=0,t[12]=0,t[13]=0,t[14]=0,t[15]=1,this}makeRotationFromQuaternion(e){return this.compose(Ii,e,Li)}lookAt(e,t,n){let r=this.elements;return Bi.subVectors(e,t),Bi.lengthSq()===0&&(Bi.z=1),Bi.normalize(),Ri.crossVectors(n,Bi),Ri.lengthSq()===0&&(Math.abs(n.z)===1?Bi.x+=1e-4:Bi.z+=1e-4,Bi.normalize(),Ri.crossVectors(n,Bi)),Ri.normalize(),zi.crossVectors(Bi,Ri),r[0]=Ri.x,r[4]=zi.x,r[8]=Bi.x,r[1]=Ri.y,r[5]=zi.y,r[9]=Bi.y,r[2]=Ri.z,r[6]=zi.z,r[10]=Bi.z,this}multiply(e){return this.multiplyMatrices(this,e)}premultiply(e){return this.multiplyMatrices(e,this)}multiplyMatrices(e,t){let n=e.elements,r=t.elements,i=this.elements,a=n[0],o=n[4],s=n[8],c=n[12],l=n[1],u=n[5],d=n[9],f=n[13],p=n[2],m=n[6],h=n[10],g=n[14],_=n[3],v=n[7],y=n[11],b=n[15],x=r[0],S=r[4],C=r[8],w=r[12],T=r[1],E=r[5],D=r[9],O=r[13],k=r[2],A=r[6],j=r[10],M=r[14],N=r[3],P=r[7],F=r[11],ee=r[15];return i[0]=a*x+o*T+s*k+c*N,i[4]=a*S+o*E+s*A+c*P,i[8]=a*C+o*D+s*j+c*F,i[12]=a*w+o*O+s*M+c*ee,i[1]=l*x+u*T+d*k+f*N,i[5]=l*S+u*E+d*A+f*P,i[9]=l*C+u*D+d*j+f*F,i[13]=l*w+u*O+d*M+f*ee,i[2]=p*x+m*T+h*k+g*N,i[6]=p*S+m*E+h*A+g*P,i[10]=p*C+m*D+h*j+g*F,i[14]=p*w+m*O+h*M+g*ee,i[3]=_*x+v*T+y*k+b*N,i[7]=_*S+v*E+y*A+b*P,i[11]=_*C+v*D+y*j+b*F,i[15]=_*w+v*O+y*M+b*ee,this}multiplyScalar(e){let t=this.elements;return t[0]*=e,t[4]*=e,t[8]*=e,t[12]*=e,t[1]*=e,t[5]*=e,t[9]*=e,t[13]*=e,t[2]*=e,t[6]*=e,t[10]*=e,t[14]*=e,t[3]*=e,t[7]*=e,t[11]*=e,t[15]*=e,this}determinant(){let e=this.elements,t=e[0],n=e[4],r=e[8],i=e[12],a=e[1],o=e[5],s=e[9],c=e[13],l=e[2],u=e[6],d=e[10],f=e[14],p=e[3],m=e[7],h=e[11],g=e[15];return p*(+i*s*u-r*c*u-i*o*d+n*c*d+r*o*f-n*s*f)+m*(+t*s*f-t*c*d+i*a*d-r*a*f+r*c*l-i*s*l)+h*(+t*c*u-t*o*f-i*a*u+n*a*f+i*o*l-n*c*l)+g*(-r*o*l-t*s*u+t*o*d+r*a*u-n*a*d+n*s*l)}transpose(){let e=this.elements,t;return t=e[1],e[1]=e[4],e[4]=t,t=e[2],e[2]=e[8],e[8]=t,t=e[6],e[6]=e[9],e[9]=t,t=e[3],e[3]=e[12],e[12]=t,t=e[7],e[7]=e[13],e[13]=t,t=e[11],e[11]=e[14],e[14]=t,this}setPosition(e,t,n){let r=this.elements;return e.isVector3?(r[12]=e.x,r[13]=e.y,r[14]=e.z):(r[12]=e,r[13]=t,r[14]=n),this}invert(){let e=this.elements,t=e[0],n=e[1],r=e[2],i=e[3],a=e[4],o=e[5],s=e[6],c=e[7],l=e[8],u=e[9],d=e[10],f=e[11],p=e[12],m=e[13],h=e[14],g=e[15],_=u*h*c-m*d*c+m*s*f-o*h*f-u*s*g+o*d*g,v=p*d*c-l*h*c-p*s*f+a*h*f+l*s*g-a*d*g,y=l*m*c-p*u*c+p*o*f-a*m*f-l*o*g+a*u*g,b=p*u*s-l*m*s-p*o*d+a*m*d+l*o*h-a*u*h,x=t*_+n*v+r*y+i*b;if(x===0)return this.set(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);let S=1/x;return e[0]=_*S,e[1]=(m*d*i-u*h*i-m*r*f+n*h*f+u*r*g-n*d*g)*S,e[2]=(o*h*i-m*s*i+m*r*c-n*h*c-o*r*g+n*s*g)*S,e[3]=(u*s*i-o*d*i-u*r*c+n*d*c+o*r*f-n*s*f)*S,e[4]=v*S,e[5]=(l*h*i-p*d*i+p*r*f-t*h*f-l*r*g+t*d*g)*S,e[6]=(p*s*i-a*h*i-p*r*c+t*h*c+a*r*g-t*s*g)*S,e[7]=(a*d*i-l*s*i+l*r*c-t*d*c-a*r*f+t*s*f)*S,e[8]=y*S,e[9]=(p*u*i-l*m*i-p*n*f+t*m*f+l*n*g-t*u*g)*S,e[10]=(a*m*i-p*o*i+p*n*c-t*m*c-a*n*g+t*o*g)*S,e[11]=(l*o*i-a*u*i-l*n*c+t*u*c+a*n*f-t*o*f)*S,e[12]=b*S,e[13]=(l*m*r-p*u*r+p*n*d-t*m*d-l*n*h+t*u*h)*S,e[14]=(p*o*r-a*m*r-p*n*s+t*m*s+a*n*h-t*o*h)*S,e[15]=(a*u*r-l*o*r+l*n*s-t*u*s-a*n*d+t*o*d)*S,this}scale(e){let t=this.elements,n=e.x,r=e.y,i=e.z;return t[0]*=n,t[4]*=r,t[8]*=i,t[1]*=n,t[5]*=r,t[9]*=i,t[2]*=n,t[6]*=r,t[10]*=i,t[3]*=n,t[7]*=r,t[11]*=i,this}getMaxScaleOnAxis(){let e=this.elements,t=e[0]*e[0]+e[1]*e[1]+e[2]*e[2],n=e[4]*e[4]+e[5]*e[5]+e[6]*e[6],r=e[8]*e[8]+e[9]*e[9]+e[10]*e[10];return Math.sqrt(Math.max(t,n,r))}makeTranslation(e,t,n){return e.isVector3?this.set(1,0,0,e.x,0,1,0,e.y,0,0,1,e.z,0,0,0,1):this.set(1,0,0,e,0,1,0,t,0,0,1,n,0,0,0,1),this}makeRotationX(e){let t=Math.cos(e),n=Math.sin(e);return this.set(1,0,0,0,0,t,-n,0,0,n,t,0,0,0,0,1),this}makeRotationY(e){let t=Math.cos(e),n=Math.sin(e);return this.set(t,0,n,0,0,1,0,0,-n,0,t,0,0,0,0,1),this}makeRotationZ(e){let t=Math.cos(e),n=Math.sin(e);return this.set(t,-n,0,0,n,t,0,0,0,0,1,0,0,0,0,1),this}makeRotationAxis(e,t){let n=Math.cos(t),r=Math.sin(t),i=1-n,a=e.x,o=e.y,s=e.z,c=i*a,l=i*o;return this.set(c*a+n,c*o-r*s,c*s+r*o,0,c*o+r*s,l*o+n,l*s-r*a,0,c*s-r*o,l*s+r*a,i*s*s+n,0,0,0,0,1),this}makeScale(e,t,n){return this.set(e,0,0,0,0,t,0,0,0,0,n,0,0,0,0,1),this}makeShear(e,t,n,r,i,a){return this.set(1,n,i,0,e,1,a,0,t,r,1,0,0,0,0,1),this}compose(e,t,n){let r=this.elements,i=t._x,a=t._y,o=t._z,s=t._w,c=i+i,l=a+a,u=o+o,d=i*c,f=i*l,p=i*u,m=a*l,h=a*u,g=o*u,_=s*c,v=s*l,y=s*u,b=n.x,x=n.y,S=n.z;return r[0]=(1-(m+g))*b,r[1]=(f+y)*b,r[2]=(p-v)*b,r[3]=0,r[4]=(f-y)*x,r[5]=(1-(d+g))*x,r[6]=(h+_)*x,r[7]=0,r[8]=(p+v)*S,r[9]=(h-_)*S,r[10]=(1-(d+m))*S,r[11]=0,r[12]=e.x,r[13]=e.y,r[14]=e.z,r[15]=1,this}decompose(e,t,n){let r=this.elements,i=Pi.set(r[0],r[1],r[2]).length(),a=Pi.set(r[4],r[5],r[6]).length(),o=Pi.set(r[8],r[9],r[10]).length();this.determinant()<0&&(i=-i),e.x=r[12],e.y=r[13],e.z=r[14],Fi.copy(this);let s=1/i,c=1/a,l=1/o;return Fi.elements[0]*=s,Fi.elements[1]*=s,Fi.elements[2]*=s,Fi.elements[4]*=c,Fi.elements[5]*=c,Fi.elements[6]*=c,Fi.elements[8]*=l,Fi.elements[9]*=l,Fi.elements[10]*=l,t.setFromRotationMatrix(Fi),n.x=i,n.y=a,n.z=o,this}makePerspective(e,t,n,r,i,a,o=rr){let s=this.elements,c=2*i/(t-e),l=2*i/(n-r),u=(t+e)/(t-e),d=(n+r)/(n-r),f,p;if(o===2e3)f=-(a+i)/(a-i),p=-2*a*i/(a-i);else if(o===2001)f=-a/(a-i),p=-a*i/(a-i);else throw Error(`THREE.Matrix4.makePerspective(): Invalid coordinate system: `+o);return s[0]=c,s[4]=0,s[8]=u,s[12]=0,s[1]=0,s[5]=l,s[9]=d,s[13]=0,s[2]=0,s[6]=0,s[10]=f,s[14]=p,s[3]=0,s[7]=0,s[11]=-1,s[15]=0,this}makeOrthographic(e,t,n,r,i,a,o=rr){let s=this.elements,c=1/(t-e),l=1/(n-r),u=1/(a-i),d=(t+e)*c,f=(n+r)*l,p,m;if(o===2e3)p=(a+i)*u,m=-2*u;else if(o===2001)p=i*u,m=-1*u;else throw Error(`THREE.Matrix4.makeOrthographic(): Invalid coordinate system: `+o);return s[0]=2*c,s[4]=0,s[8]=0,s[12]=-d,s[1]=0,s[5]=2*l,s[9]=0,s[13]=-f,s[2]=0,s[6]=0,s[10]=m,s[14]=-p,s[3]=0,s[7]=0,s[11]=0,s[15]=1,this}equals(e){let t=this.elements,n=e.elements;for(let e=0;e<16;e++)if(t[e]!==n[e])return!1;return!0}fromArray(e,t=0){for(let n=0;n<16;n++)this.elements[n]=e[n+t];return this}toArray(e=[],t=0){let n=this.elements;return e[t]=n[0],e[t+1]=n[1],e[t+2]=n[2],e[t+3]=n[3],e[t+4]=n[4],e[t+5]=n[5],e[t+6]=n[6],e[t+7]=n[7],e[t+8]=n[8],e[t+9]=n[9],e[t+10]=n[10],e[t+11]=n[11],e[t+12]=n[12],e[t+13]=n[13],e[t+14]=n[14],e[t+15]=n[15],e}},Pi=new Z,Fi=new Ni,Ii=new Z(0,0,0),Li=new Z(1,1,1),Ri=new Z,zi=new Z,Bi=new Z,Vi=new Ni,Hi=new ri,Ui=class e{constructor(t=0,n=0,r=0,i=e.DEFAULT_ORDER){this.isEuler=!0,this._x=t,this._y=n,this._z=r,this._order=i}get x(){return this._x}set x(e){this._x=e,this._onChangeCallback()}get y(){return this._y}set y(e){this._y=e,this._onChangeCallback()}get z(){return this._z}set z(e){this._z=e,this._onChangeCallback()}get order(){return this._order}set order(e){this._order=e,this._onChangeCallback()}set(e,t,n,r=this._order){return this._x=e,this._y=t,this._z=n,this._order=r,this._onChangeCallback(),this}clone(){return new this.constructor(this._x,this._y,this._z,this._order)}copy(e){return this._x=e._x,this._y=e._y,this._z=e._z,this._order=e._order,this._onChangeCallback(),this}setFromRotationMatrix(e,t=this._order,n=!0){let r=e.elements,i=r[0],a=r[4],o=r[8],s=r[1],c=r[5],l=r[9],u=r[2],d=r[6],f=r[10];switch(t){case`XYZ`:this._y=Math.asin(ur(o,-1,1)),Math.abs(o)<.9999999?(this._x=Math.atan2(-l,f),this._z=Math.atan2(-a,i)):(this._x=Math.atan2(d,c),this._z=0);break;case`YXZ`:this._x=Math.asin(-ur(l,-1,1)),Math.abs(l)<.9999999?(this._y=Math.atan2(o,f),this._z=Math.atan2(s,c)):(this._y=Math.atan2(-u,i),this._z=0);break;case`ZXY`:this._x=Math.asin(ur(d,-1,1)),Math.abs(d)<.9999999?(this._y=Math.atan2(-u,f),this._z=Math.atan2(-a,c)):(this._y=0,this._z=Math.atan2(s,i));break;case`ZYX`:this._y=Math.asin(-ur(u,-1,1)),Math.abs(u)<.9999999?(this._x=Math.atan2(d,f),this._z=Math.atan2(s,i)):(this._x=0,this._z=Math.atan2(-a,c));break;case`YZX`:this._z=Math.asin(ur(s,-1,1)),Math.abs(s)<.9999999?(this._x=Math.atan2(-l,c),this._y=Math.atan2(-u,i)):(this._x=0,this._y=Math.atan2(o,f));break;case`XZY`:this._z=Math.asin(-ur(a,-1,1)),Math.abs(a)<.9999999?(this._x=Math.atan2(d,c),this._y=Math.atan2(o,i)):(this._x=Math.atan2(-l,f),this._y=0);break;default:console.warn(`THREE.Euler: .setFromRotationMatrix() encountered an unknown order: `+t)}return this._order=t,n===!0&&this._onChangeCallback(),this}setFromQuaternion(e,t,n){return Vi.makeRotationFromQuaternion(e),this.setFromRotationMatrix(Vi,t,n)}setFromVector3(e,t=this._order){return this.set(e.x,e.y,e.z,t)}reorder(e){return Hi.setFromEuler(this),this.setFromQuaternion(Hi,e)}equals(e){return e._x===this._x&&e._y===this._y&&e._z===this._z&&e._order===this._order}fromArray(e){return this._x=e[0],this._y=e[1],this._z=e[2],e[3]!==void 0&&(this._order=e[3]),this._onChangeCallback(),this}toArray(e=[],t=0){return e[t]=this._x,e[t+1]=this._y,e[t+2]=this._z,e[t+3]=this._order,e}_onChange(e){return this._onChangeCallback=e,this}_onChangeCallback(){}*[Symbol.iterator](){yield this._x,yield this._y,yield this._z,yield this._order}};Ui.DEFAULT_ORDER=`XYZ`;var Wi=class{constructor(){this.mask=1}set(e){this.mask=(1<<e|0)>>>0}enable(e){this.mask|=1<<e|0}enableAll(){this.mask=-1}toggle(e){this.mask^=1<<e|0}disable(e){this.mask&=~(1<<e|0)}disableAll(){this.mask=0}test(e){return(this.mask&e.mask)!==0}isEnabled(e){return(this.mask&(1<<e|0))!=0}},Gi=0,Ki=new Z,qi=new ri,Ji=new Ni,Yi=new Z,Xi=new Z,Zi=new Z,Qi=new ri,$i=new Z(1,0,0),ea=new Z(0,1,0),ta=new Z(0,0,1),na={type:`added`},ra={type:`removed`},ia={type:`childadded`,child:null},aa={type:`childremoved`,child:null},oa=class e extends ir{constructor(){super(),this.isObject3D=!0,Object.defineProperty(this,`id`,{value:Gi++}),this.uuid=lr(),this.name=``,this.type=`Object3D`,this.parent=null,this.children=[],this.up=e.DEFAULT_UP.clone();let t=new Z,n=new Ui,r=new ri,i=new Z(1,1,1);function a(){r.setFromEuler(n,!1)}function o(){n.setFromQuaternion(r,void 0,!1)}n._onChange(a),r._onChange(o),Object.defineProperties(this,{position:{configurable:!0,enumerable:!0,value:t},rotation:{configurable:!0,enumerable:!0,value:n},quaternion:{configurable:!0,enumerable:!0,value:r},scale:{configurable:!0,enumerable:!0,value:i},modelViewMatrix:{value:new Ni},normalMatrix:{value:new X}}),this.matrix=new Ni,this.matrixWorld=new Ni,this.matrixAutoUpdate=e.DEFAULT_MATRIX_AUTO_UPDATE,this.matrixWorldAutoUpdate=e.DEFAULT_MATRIX_WORLD_AUTO_UPDATE,this.matrixWorldNeedsUpdate=!1,this.layers=new Wi,this.visible=!0,this.castShadow=!1,this.receiveShadow=!1,this.frustumCulled=!0,this.renderOrder=0,this.animations=[],this.userData={}}onBeforeShadow(){}onAfterShadow(){}onBeforeRender(){}onAfterRender(){}applyMatrix4(e){this.matrixAutoUpdate&&this.updateMatrix(),this.matrix.premultiply(e),this.matrix.decompose(this.position,this.quaternion,this.scale)}applyQuaternion(e){return this.quaternion.premultiply(e),this}setRotationFromAxisAngle(e,t){this.quaternion.setFromAxisAngle(e,t)}setRotationFromEuler(e){this.quaternion.setFromEuler(e,!0)}setRotationFromMatrix(e){this.quaternion.setFromRotationMatrix(e)}setRotationFromQuaternion(e){this.quaternion.copy(e)}rotateOnAxis(e,t){return qi.setFromAxisAngle(e,t),this.quaternion.multiply(qi),this}rotateOnWorldAxis(e,t){return qi.setFromAxisAngle(e,t),this.quaternion.premultiply(qi),this}rotateX(e){return this.rotateOnAxis($i,e)}rotateY(e){return this.rotateOnAxis(ea,e)}rotateZ(e){return this.rotateOnAxis(ta,e)}translateOnAxis(e,t){return Ki.copy(e).applyQuaternion(this.quaternion),this.position.add(Ki.multiplyScalar(t)),this}translateX(e){return this.translateOnAxis($i,e)}translateY(e){return this.translateOnAxis(ea,e)}translateZ(e){return this.translateOnAxis(ta,e)}localToWorld(e){return this.updateWorldMatrix(!0,!1),e.applyMatrix4(this.matrixWorld)}worldToLocal(e){return this.updateWorldMatrix(!0,!1),e.applyMatrix4(Ji.copy(this.matrixWorld).invert())}lookAt(e,t,n){e.isVector3?Yi.copy(e):Yi.set(e,t,n);let r=this.parent;this.updateWorldMatrix(!0,!1),Xi.setFromMatrixPosition(this.matrixWorld),this.isCamera||this.isLight?Ji.lookAt(Xi,Yi,this.up):Ji.lookAt(Yi,Xi,this.up),this.quaternion.setFromRotationMatrix(Ji),r&&(Ji.extractRotation(r.matrixWorld),qi.setFromRotationMatrix(Ji),this.quaternion.premultiply(qi.invert()))}add(e){if(arguments.length>1){for(let e=0;e<arguments.length;e++)this.add(arguments[e]);return this}return e===this?(console.error(`THREE.Object3D.add: object can't be added as a child of itself.`,e),this):(e&&e.isObject3D?(e.parent!==null&&e.parent.remove(e),e.parent=this,this.children.push(e),e.dispatchEvent(na),ia.child=e,this.dispatchEvent(ia),ia.child=null):console.error(`THREE.Object3D.add: object not an instance of THREE.Object3D.`,e),this)}remove(e){if(arguments.length>1){for(let e=0;e<arguments.length;e++)this.remove(arguments[e]);return this}let t=this.children.indexOf(e);return t!==-1&&(e.parent=null,this.children.splice(t,1),e.dispatchEvent(ra),aa.child=e,this.dispatchEvent(aa),aa.child=null),this}removeFromParent(){let e=this.parent;return e!==null&&e.remove(this),this}clear(){return this.remove(...this.children)}attach(e){return this.updateWorldMatrix(!0,!1),Ji.copy(this.matrixWorld).invert(),e.parent!==null&&(e.parent.updateWorldMatrix(!0,!1),Ji.multiply(e.parent.matrixWorld)),e.applyMatrix4(Ji),this.add(e),e.updateWorldMatrix(!1,!0),this}getObjectById(e){return this.getObjectByProperty(`id`,e)}getObjectByName(e){return this.getObjectByProperty(`name`,e)}getObjectByProperty(e,t){if(this[e]===t)return this;for(let n=0,r=this.children.length;n<r;n++){let r=this.children[n].getObjectByProperty(e,t);if(r!==void 0)return r}}getObjectsByProperty(e,t,n=[]){this[e]===t&&n.push(this);let r=this.children;for(let i=0,a=r.length;i<a;i++)r[i].getObjectsByProperty(e,t,n);return n}getWorldPosition(e){return this.updateWorldMatrix(!0,!1),e.setFromMatrixPosition(this.matrixWorld)}getWorldQuaternion(e){return this.updateWorldMatrix(!0,!1),this.matrixWorld.decompose(Xi,e,Zi),e}getWorldScale(e){return this.updateWorldMatrix(!0,!1),this.matrixWorld.decompose(Xi,Qi,e),e}getWorldDirection(e){this.updateWorldMatrix(!0,!1);let t=this.matrixWorld.elements;return e.set(t[8],t[9],t[10]).normalize()}raycast(){}traverse(e){e(this);let t=this.children;for(let n=0,r=t.length;n<r;n++)t[n].traverse(e)}traverseVisible(e){if(this.visible===!1)return;e(this);let t=this.children;for(let n=0,r=t.length;n<r;n++)t[n].traverseVisible(e)}traverseAncestors(e){let t=this.parent;t!==null&&(e(t),t.traverseAncestors(e))}updateMatrix(){this.matrix.compose(this.position,this.quaternion,this.scale),this.matrixWorldNeedsUpdate=!0}updateMatrixWorld(e){this.matrixAutoUpdate&&this.updateMatrix(),(this.matrixWorldNeedsUpdate||e)&&(this.parent===null?this.matrixWorld.copy(this.matrix):this.matrixWorld.multiplyMatrices(this.parent.matrixWorld,this.matrix),this.matrixWorldNeedsUpdate=!1,e=!0);let t=this.children;for(let n=0,r=t.length;n<r;n++){let r=t[n];(r.matrixWorldAutoUpdate===!0||e===!0)&&r.updateMatrixWorld(e)}}updateWorldMatrix(e,t){let n=this.parent;if(e===!0&&n!==null&&n.matrixWorldAutoUpdate===!0&&n.updateWorldMatrix(!0,!1),this.matrixAutoUpdate&&this.updateMatrix(),this.parent===null?this.matrixWorld.copy(this.matrix):this.matrixWorld.multiplyMatrices(this.parent.matrixWorld,this.matrix),t===!0){let e=this.children;for(let t=0,n=e.length;t<n;t++){let n=e[t];n.matrixWorldAutoUpdate===!0&&n.updateWorldMatrix(!1,!0)}}}toJSON(e){let t=e===void 0||typeof e==`string`,n={};t&&(e={geometries:{},materials:{},textures:{},images:{},shapes:{},skeletons:{},animations:{},nodes:{}},n.metadata={version:4.6,type:`Object`,generator:`Object3D.toJSON`});let r={};r.uuid=this.uuid,r.type=this.type,this.name!==``&&(r.name=this.name),this.castShadow===!0&&(r.castShadow=!0),this.receiveShadow===!0&&(r.receiveShadow=!0),this.visible===!1&&(r.visible=!1),this.frustumCulled===!1&&(r.frustumCulled=!1),this.renderOrder!==0&&(r.renderOrder=this.renderOrder),Object.keys(this.userData).length>0&&(r.userData=this.userData),r.layers=this.layers.mask,r.matrix=this.matrix.toArray(),r.up=this.up.toArray(),this.matrixAutoUpdate===!1&&(r.matrixAutoUpdate=!1),this.isInstancedMesh&&(r.type=`InstancedMesh`,r.count=this.count,r.instanceMatrix=this.instanceMatrix.toJSON(),this.instanceColor!==null&&(r.instanceColor=this.instanceColor.toJSON())),this.isBatchedMesh&&(r.type=`BatchedMesh`,r.perObjectFrustumCulled=this.perObjectFrustumCulled,r.sortObjects=this.sortObjects,r.drawRanges=this._drawRanges,r.reservedRanges=this._reservedRanges,r.visibility=this._visibility,r.active=this._active,r.bounds=this._bounds.map(e=>({boxInitialized:e.boxInitialized,boxMin:e.box.min.toArray(),boxMax:e.box.max.toArray(),sphereInitialized:e.sphereInitialized,sphereRadius:e.sphere.radius,sphereCenter:e.sphere.center.toArray()})),r.maxGeometryCount=this._maxGeometryCount,r.maxVertexCount=this._maxVertexCount,r.maxIndexCount=this._maxIndexCount,r.geometryInitialized=this._geometryInitialized,r.geometryCount=this._geometryCount,r.matricesTexture=this._matricesTexture.toJSON(e),this.boundingSphere!==null&&(r.boundingSphere={center:r.boundingSphere.center.toArray(),radius:r.boundingSphere.radius}),this.boundingBox!==null&&(r.boundingBox={min:r.boundingBox.min.toArray(),max:r.boundingBox.max.toArray()}));function i(t,n){return t[n.uuid]===void 0&&(t[n.uuid]=n.toJSON(e)),n.uuid}if(this.isScene)this.background&&(this.background.isColor?r.background=this.background.toJSON():this.background.isTexture&&(r.background=this.background.toJSON(e).uuid)),this.environment&&this.environment.isTexture&&this.environment.isRenderTargetTexture!==!0&&(r.environment=this.environment.toJSON(e).uuid);else if(this.isMesh||this.isLine||this.isPoints){r.geometry=i(e.geometries,this.geometry);let t=this.geometry.parameters;if(t!==void 0&&t.shapes!==void 0){let n=t.shapes;if(Array.isArray(n))for(let t=0,r=n.length;t<r;t++){let r=n[t];i(e.shapes,r)}else i(e.shapes,n)}}if(this.isSkinnedMesh&&(r.bindMode=this.bindMode,r.bindMatrix=this.bindMatrix.toArray(),this.skeleton!==void 0&&(i(e.skeletons,this.skeleton),r.skeleton=this.skeleton.uuid)),this.material!==void 0)if(Array.isArray(this.material)){let t=[];for(let n=0,r=this.material.length;n<r;n++)t.push(i(e.materials,this.material[n]));r.material=t}else r.material=i(e.materials,this.material);if(this.children.length>0){r.children=[];for(let t=0;t<this.children.length;t++)r.children.push(this.children[t].toJSON(e).object)}if(this.animations.length>0){r.animations=[];for(let t=0;t<this.animations.length;t++){let n=this.animations[t];r.animations.push(i(e.animations,n))}}if(t){let t=a(e.geometries),r=a(e.materials),i=a(e.textures),o=a(e.images),s=a(e.shapes),c=a(e.skeletons),l=a(e.animations),u=a(e.nodes);t.length>0&&(n.geometries=t),r.length>0&&(n.materials=r),i.length>0&&(n.textures=i),o.length>0&&(n.images=o),s.length>0&&(n.shapes=s),c.length>0&&(n.skeletons=c),l.length>0&&(n.animations=l),u.length>0&&(n.nodes=u)}return n.object=r,n;function a(e){let t=[];for(let n in e){let r=e[n];delete r.metadata,t.push(r)}return t}}clone(e){return new this.constructor().copy(this,e)}copy(e,t=!0){if(this.name=e.name,this.up.copy(e.up),this.position.copy(e.position),this.rotation.order=e.rotation.order,this.quaternion.copy(e.quaternion),this.scale.copy(e.scale),this.matrix.copy(e.matrix),this.matrixWorld.copy(e.matrixWorld),this.matrixAutoUpdate=e.matrixAutoUpdate,this.matrixWorldAutoUpdate=e.matrixWorldAutoUpdate,this.matrixWorldNeedsUpdate=e.matrixWorldNeedsUpdate,this.layers.mask=e.layers.mask,this.visible=e.visible,this.castShadow=e.castShadow,this.receiveShadow=e.receiveShadow,this.frustumCulled=e.frustumCulled,this.renderOrder=e.renderOrder,this.animations=e.animations.slice(),this.userData=JSON.parse(JSON.stringify(e.userData)),t===!0)for(let t=0;t<e.children.length;t++){let n=e.children[t];this.add(n.clone())}return this}};oa.DEFAULT_UP=new Z(0,1,0),oa.DEFAULT_MATRIX_AUTO_UPDATE=!0,oa.DEFAULT_MATRIX_WORLD_AUTO_UPDATE=!0;var sa=new Z,ca=new Z,la=new Z,ua=new Z,da=new Z,fa=new Z,pa=new Z,ma=new Z,ha=new Z,ga=new Z,_a=class e{constructor(e=new Z,t=new Z,n=new Z){this.a=e,this.b=t,this.c=n}static getNormal(e,t,n,r){r.subVectors(n,t),sa.subVectors(e,t),r.cross(sa);let i=r.lengthSq();return i>0?r.multiplyScalar(1/Math.sqrt(i)):r.set(0,0,0)}static getBarycoord(e,t,n,r,i){sa.subVectors(r,t),ca.subVectors(n,t),la.subVectors(e,t);let a=sa.dot(sa),o=sa.dot(ca),s=sa.dot(la),c=ca.dot(ca),l=ca.dot(la),u=a*c-o*o;if(u===0)return i.set(0,0,0),null;let d=1/u,f=(c*s-o*l)*d,p=(a*l-o*s)*d;return i.set(1-f-p,p,f)}static containsPoint(e,t,n,r){return this.getBarycoord(e,t,n,r,ua)===null?!1:ua.x>=0&&ua.y>=0&&ua.x+ua.y<=1}static getInterpolation(e,t,n,r,i,a,o,s){return this.getBarycoord(e,t,n,r,ua)===null?(s.x=0,s.y=0,`z`in s&&(s.z=0),`w`in s&&(s.w=0),null):(s.setScalar(0),s.addScaledVector(i,ua.x),s.addScaledVector(a,ua.y),s.addScaledVector(o,ua.z),s)}static isFrontFacing(e,t,n,r){return sa.subVectors(n,t),ca.subVectors(e,t),sa.cross(ca).dot(r)<0}set(e,t,n){return this.a.copy(e),this.b.copy(t),this.c.copy(n),this}setFromPointsAndIndices(e,t,n,r){return this.a.copy(e[t]),this.b.copy(e[n]),this.c.copy(e[r]),this}setFromAttributeAndIndices(e,t,n,r){return this.a.fromBufferAttribute(e,t),this.b.fromBufferAttribute(e,n),this.c.fromBufferAttribute(e,r),this}clone(){return new this.constructor().copy(this)}copy(e){return this.a.copy(e.a),this.b.copy(e.b),this.c.copy(e.c),this}getArea(){return sa.subVectors(this.c,this.b),ca.subVectors(this.a,this.b),sa.cross(ca).length()*.5}getMidpoint(e){return e.addVectors(this.a,this.b).add(this.c).multiplyScalar(1/3)}getNormal(t){return e.getNormal(this.a,this.b,this.c,t)}getPlane(e){return e.setFromCoplanarPoints(this.a,this.b,this.c)}getBarycoord(t,n){return e.getBarycoord(t,this.a,this.b,this.c,n)}getInterpolation(t,n,r,i,a){return e.getInterpolation(t,this.a,this.b,this.c,n,r,i,a)}containsPoint(t){return e.containsPoint(t,this.a,this.b,this.c)}isFrontFacing(t){return e.isFrontFacing(this.a,this.b,this.c,t)}intersectsBox(e){return e.intersectsTriangle(this)}closestPointToPoint(e,t){let n=this.a,r=this.b,i=this.c,a,o;da.subVectors(r,n),fa.subVectors(i,n),ma.subVectors(e,n);let s=da.dot(ma),c=fa.dot(ma);if(s<=0&&c<=0)return t.copy(n);ha.subVectors(e,r);let l=da.dot(ha),u=fa.dot(ha);if(l>=0&&u<=l)return t.copy(r);let d=s*u-l*c;if(d<=0&&s>=0&&l<=0)return a=s/(s-l),t.copy(n).addScaledVector(da,a);ga.subVectors(e,i);let f=da.dot(ga),p=fa.dot(ga);if(p>=0&&f<=p)return t.copy(i);let m=f*c-s*p;if(m<=0&&c>=0&&p<=0)return o=c/(c-p),t.copy(n).addScaledVector(fa,o);let h=l*p-f*u;if(h<=0&&u-l>=0&&f-p>=0)return pa.subVectors(i,r),o=(u-l)/(u-l+(f-p)),t.copy(r).addScaledVector(pa,o);let g=1/(h+m+d);return a=m*g,o=d*g,t.copy(n).addScaledVector(da,a).addScaledVector(fa,o)}equals(e){return e.a.equals(this.a)&&e.b.equals(this.b)&&e.c.equals(this.c)}},va={aliceblue:15792383,antiquewhite:16444375,aqua:65535,aquamarine:8388564,azure:15794175,beige:16119260,bisque:16770244,black:0,blanchedalmond:16772045,blue:255,blueviolet:9055202,brown:10824234,burlywood:14596231,cadetblue:6266528,chartreuse:8388352,chocolate:13789470,coral:16744272,cornflowerblue:6591981,cornsilk:16775388,crimson:14423100,cyan:65535,darkblue:139,darkcyan:35723,darkgoldenrod:12092939,darkgray:11119017,darkgreen:25600,darkgrey:11119017,darkkhaki:12433259,darkmagenta:9109643,darkolivegreen:5597999,darkorange:16747520,darkorchid:10040012,darkred:9109504,darksalmon:15308410,darkseagreen:9419919,darkslateblue:4734347,darkslategray:3100495,darkslategrey:3100495,darkturquoise:52945,darkviolet:9699539,deeppink:16716947,deepskyblue:49151,dimgray:6908265,dimgrey:6908265,dodgerblue:2003199,firebrick:11674146,floralwhite:16775920,forestgreen:2263842,fuchsia:16711935,gainsboro:14474460,ghostwhite:16316671,gold:16766720,goldenrod:14329120,gray:8421504,green:32768,greenyellow:11403055,grey:8421504,honeydew:15794160,hotpink:16738740,indianred:13458524,indigo:4915330,ivory:16777200,khaki:15787660,lavender:15132410,lavenderblush:16773365,lawngreen:8190976,lemonchiffon:16775885,lightblue:11393254,lightcoral:15761536,lightcyan:14745599,lightgoldenrodyellow:16448210,lightgray:13882323,lightgreen:9498256,lightgrey:13882323,lightpink:16758465,lightsalmon:16752762,lightseagreen:2142890,lightskyblue:8900346,lightslategray:7833753,lightslategrey:7833753,lightsteelblue:11584734,lightyellow:16777184,lime:65280,limegreen:3329330,linen:16445670,magenta:16711935,maroon:8388608,mediumaquamarine:6737322,mediumblue:205,mediumorchid:12211667,mediumpurple:9662683,mediumseagreen:3978097,mediumslateblue:8087790,mediumspringgreen:64154,mediumturquoise:4772300,mediumvioletred:13047173,midnightblue:1644912,mintcream:16121850,mistyrose:16770273,moccasin:16770229,navajowhite:16768685,navy:128,oldlace:16643558,olive:8421376,olivedrab:7048739,orange:16753920,orangered:16729344,orchid:14315734,palegoldenrod:15657130,palegreen:10025880,paleturquoise:11529966,palevioletred:14381203,papayawhip:16773077,peachpuff:16767673,peru:13468991,pink:16761035,plum:14524637,powderblue:11591910,purple:8388736,rebeccapurple:6697881,red:16711680,rosybrown:12357519,royalblue:4286945,saddlebrown:9127187,salmon:16416882,sandybrown:16032864,seagreen:3050327,seashell:16774638,sienna:10506797,silver:12632256,skyblue:8900331,slateblue:6970061,slategray:7372944,slategrey:7372944,snow:16775930,springgreen:65407,steelblue:4620980,tan:13808780,teal:32896,thistle:14204888,tomato:16737095,turquoise:4251856,violet:15631086,wheat:16113331,white:16777215,whitesmoke:16119285,yellow:16776960,yellowgreen:10145074},ya={h:0,s:0,l:0},ba={h:0,s:0,l:0};function xa(e,t,n){return n<0&&(n+=1),n>1&&--n,n<1/6?e+(t-e)*6*n:n<1/2?t:n<2/3?e+(t-e)*6*(2/3-n):e}var Sa=class{constructor(e,t,n){return this.isColor=!0,this.r=1,this.g=1,this.b=1,this.set(e,t,n)}set(e,t,n){if(t===void 0&&n===void 0){let t=e;t&&t.isColor?this.copy(t):typeof t==`number`?this.setHex(t):typeof t==`string`&&this.setStyle(t)}else this.setRGB(e,t,n);return this}setScalar(e){return this.r=e,this.g=e,this.b=e,this}setHex(e,t=qn){return e=Math.floor(e),this.r=(e>>16&255)/255,this.g=(e>>8&255)/255,this.b=(e&255)/255,Hr.toWorkingColorSpace(this,t),this}setRGB(e,t,n,r=Hr.workingColorSpace){return this.r=e,this.g=t,this.b=n,Hr.toWorkingColorSpace(this,r),this}setHSL(e,t,n,r=Hr.workingColorSpace){if(e=dr(e,1),t=ur(t,0,1),n=ur(n,0,1),t===0)this.r=this.g=this.b=n;else{let r=n<=.5?n*(1+t):n+t-n*t,i=2*n-r;this.r=xa(i,r,e+1/3),this.g=xa(i,r,e),this.b=xa(i,r,e-1/3)}return Hr.toWorkingColorSpace(this,r),this}setStyle(e,t=qn){function n(t){t!==void 0&&parseFloat(t)<1&&console.warn(`THREE.Color: Alpha component of `+e+` will be ignored.`)}let r;if(r=/^(\w+)\(([^\)]*)\)/.exec(e)){let i,a=r[1],o=r[2];switch(a){case`rgb`:case`rgba`:if(i=/^\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*(\d*\.?\d+)\s*)?$/.exec(o))return n(i[4]),this.setRGB(Math.min(255,parseInt(i[1],10))/255,Math.min(255,parseInt(i[2],10))/255,Math.min(255,parseInt(i[3],10))/255,t);if(i=/^\s*(\d+)\%\s*,\s*(\d+)\%\s*,\s*(\d+)\%\s*(?:,\s*(\d*\.?\d+)\s*)?$/.exec(o))return n(i[4]),this.setRGB(Math.min(100,parseInt(i[1],10))/100,Math.min(100,parseInt(i[2],10))/100,Math.min(100,parseInt(i[3],10))/100,t);break;case`hsl`:case`hsla`:if(i=/^\s*(\d*\.?\d+)\s*,\s*(\d*\.?\d+)\%\s*,\s*(\d*\.?\d+)\%\s*(?:,\s*(\d*\.?\d+)\s*)?$/.exec(o))return n(i[4]),this.setHSL(parseFloat(i[1])/360,parseFloat(i[2])/100,parseFloat(i[3])/100,t);break;default:console.warn(`THREE.Color: Unknown color model `+e)}}else if(r=/^\#([A-Fa-f\d]+)$/.exec(e)){let n=r[1],i=n.length;if(i===3)return this.setRGB(parseInt(n.charAt(0),16)/15,parseInt(n.charAt(1),16)/15,parseInt(n.charAt(2),16)/15,t);if(i===6)return this.setHex(parseInt(n,16),t);console.warn(`THREE.Color: Invalid hex color `+e)}else if(e&&e.length>0)return this.setColorName(e,t);return this}setColorName(e,t=qn){let n=va[e.toLowerCase()];return n===void 0?console.warn(`THREE.Color: Unknown color `+e):this.setHex(n,t),this}clone(){return new this.constructor(this.r,this.g,this.b)}copy(e){return this.r=e.r,this.g=e.g,this.b=e.b,this}copySRGBToLinear(e){return this.r=Ur(e.r),this.g=Ur(e.g),this.b=Ur(e.b),this}copyLinearToSRGB(e){return this.r=Wr(e.r),this.g=Wr(e.g),this.b=Wr(e.b),this}convertSRGBToLinear(){return this.copySRGBToLinear(this),this}convertLinearToSRGB(){return this.copyLinearToSRGB(this),this}getHex(e=qn){return Hr.fromWorkingColorSpace(Ca.copy(this),e),Math.round(ur(Ca.r*255,0,255))*65536+Math.round(ur(Ca.g*255,0,255))*256+Math.round(ur(Ca.b*255,0,255))}getHexString(e=qn){return(`000000`+this.getHex(e).toString(16)).slice(-6)}getHSL(e,t=Hr.workingColorSpace){Hr.fromWorkingColorSpace(Ca.copy(this),t);let n=Ca.r,r=Ca.g,i=Ca.b,a=Math.max(n,r,i),o=Math.min(n,r,i),s,c,l=(o+a)/2;if(o===a)s=0,c=0;else{let e=a-o;switch(c=l<=.5?e/(a+o):e/(2-a-o),a){case n:s=(r-i)/e+(r<i?6:0);break;case r:s=(i-n)/e+2;break;case i:s=(n-r)/e+4;break}s/=6}return e.h=s,e.s=c,e.l=l,e}getRGB(e,t=Hr.workingColorSpace){return Hr.fromWorkingColorSpace(Ca.copy(this),t),e.r=Ca.r,e.g=Ca.g,e.b=Ca.b,e}getStyle(e=qn){Hr.fromWorkingColorSpace(Ca.copy(this),e);let t=Ca.r,n=Ca.g,r=Ca.b;return e===`srgb`?`rgb(${Math.round(t*255)},${Math.round(n*255)},${Math.round(r*255)})`:`color(${e} ${t.toFixed(3)} ${n.toFixed(3)} ${r.toFixed(3)})`}offsetHSL(e,t,n){return this.getHSL(ya),this.setHSL(ya.h+e,ya.s+t,ya.l+n)}add(e){return this.r+=e.r,this.g+=e.g,this.b+=e.b,this}addColors(e,t){return this.r=e.r+t.r,this.g=e.g+t.g,this.b=e.b+t.b,this}addScalar(e){return this.r+=e,this.g+=e,this.b+=e,this}sub(e){return this.r=Math.max(0,this.r-e.r),this.g=Math.max(0,this.g-e.g),this.b=Math.max(0,this.b-e.b),this}multiply(e){return this.r*=e.r,this.g*=e.g,this.b*=e.b,this}multiplyScalar(e){return this.r*=e,this.g*=e,this.b*=e,this}lerp(e,t){return this.r+=(e.r-this.r)*t,this.g+=(e.g-this.g)*t,this.b+=(e.b-this.b)*t,this}lerpColors(e,t,n){return this.r=e.r+(t.r-e.r)*n,this.g=e.g+(t.g-e.g)*n,this.b=e.b+(t.b-e.b)*n,this}lerpHSL(e,t){this.getHSL(ya),e.getHSL(ba);let n=mr(ya.h,ba.h,t),r=mr(ya.s,ba.s,t),i=mr(ya.l,ba.l,t);return this.setHSL(n,r,i),this}setFromVector3(e){return this.r=e.x,this.g=e.y,this.b=e.z,this}applyMatrix3(e){let t=this.r,n=this.g,r=this.b,i=e.elements;return this.r=i[0]*t+i[3]*n+i[6]*r,this.g=i[1]*t+i[4]*n+i[7]*r,this.b=i[2]*t+i[5]*n+i[8]*r,this}equals(e){return e.r===this.r&&e.g===this.g&&e.b===this.b}fromArray(e,t=0){return this.r=e[t],this.g=e[t+1],this.b=e[t+2],this}toArray(e=[],t=0){return e[t]=this.r,e[t+1]=this.g,e[t+2]=this.b,e}fromBufferAttribute(e,t){return this.r=e.getX(t),this.g=e.getY(t),this.b=e.getZ(t),this}toJSON(){return this.getHex()}*[Symbol.iterator](){yield this.r,yield this.g,yield this.b}},Ca=new Sa;Sa.NAMES=va;var wa=0,Ta=class extends ir{constructor(){super(),this.isMaterial=!0,Object.defineProperty(this,`id`,{value:wa++}),this.uuid=lr(),this.name=``,this.type=`Material`,this.blending=1,this.side=0,this.vertexColors=!1,this.opacity=1,this.transparent=!1,this.alphaHash=!1,this.blendSrc=204,this.blendDst=205,this.blendEquation=100,this.blendSrcAlpha=null,this.blendDstAlpha=null,this.blendEquationAlpha=null,this.blendColor=new Sa(0,0,0),this.blendAlpha=0,this.depthFunc=3,this.depthTest=!0,this.depthWrite=!0,this.stencilWriteMask=255,this.stencilFunc=519,this.stencilRef=0,this.stencilFuncMask=255,this.stencilFail=er,this.stencilZFail=er,this.stencilZPass=er,this.stencilWrite=!1,this.clippingPlanes=null,this.clipIntersection=!1,this.clipShadows=!1,this.shadowSide=null,this.colorWrite=!0,this.precision=null,this.polygonOffset=!1,this.polygonOffsetFactor=0,this.polygonOffsetUnits=0,this.dithering=!1,this.alphaToCoverage=!1,this.premultipliedAlpha=!1,this.forceSinglePass=!1,this.visible=!0,this.toneMapped=!0,this.userData={},this.version=0,this._alphaTest=0}get alphaTest(){return this._alphaTest}set alphaTest(e){this._alphaTest>0!=e>0&&this.version++,this._alphaTest=e}onBuild(){}onBeforeRender(){}onBeforeCompile(){}customProgramCacheKey(){return this.onBeforeCompile.toString()}setValues(e){if(e!==void 0)for(let t in e){let n=e[t];if(n===void 0){console.warn(`THREE.Material: parameter '${t}' has value of undefined.`);continue}let r=this[t];if(r===void 0){console.warn(`THREE.Material: '${t}' is not a property of THREE.${this.type}.`);continue}r&&r.isColor?r.set(n):r&&r.isVector3&&n&&n.isVector3?r.copy(n):this[t]=n}}toJSON(e){let t=e===void 0||typeof e==`string`;t&&(e={textures:{},images:{}});let n={metadata:{version:4.6,type:`Material`,generator:`Material.toJSON`}};n.uuid=this.uuid,n.type=this.type,this.name!==``&&(n.name=this.name),this.color&&this.color.isColor&&(n.color=this.color.getHex()),this.roughness!==void 0&&(n.roughness=this.roughness),this.metalness!==void 0&&(n.metalness=this.metalness),this.sheen!==void 0&&(n.sheen=this.sheen),this.sheenColor&&this.sheenColor.isColor&&(n.sheenColor=this.sheenColor.getHex()),this.sheenRoughness!==void 0&&(n.sheenRoughness=this.sheenRoughness),this.emissive&&this.emissive.isColor&&(n.emissive=this.emissive.getHex()),this.emissiveIntensity!==void 0&&this.emissiveIntensity!==1&&(n.emissiveIntensity=this.emissiveIntensity),this.specular&&this.specular.isColor&&(n.specular=this.specular.getHex()),this.specularIntensity!==void 0&&(n.specularIntensity=this.specularIntensity),this.specularColor&&this.specularColor.isColor&&(n.specularColor=this.specularColor.getHex()),this.shininess!==void 0&&(n.shininess=this.shininess),this.clearcoat!==void 0&&(n.clearcoat=this.clearcoat),this.clearcoatRoughness!==void 0&&(n.clearcoatRoughness=this.clearcoatRoughness),this.clearcoatMap&&this.clearcoatMap.isTexture&&(n.clearcoatMap=this.clearcoatMap.toJSON(e).uuid),this.clearcoatRoughnessMap&&this.clearcoatRoughnessMap.isTexture&&(n.clearcoatRoughnessMap=this.clearcoatRoughnessMap.toJSON(e).uuid),this.clearcoatNormalMap&&this.clearcoatNormalMap.isTexture&&(n.clearcoatNormalMap=this.clearcoatNormalMap.toJSON(e).uuid,n.clearcoatNormalScale=this.clearcoatNormalScale.toArray()),this.iridescence!==void 0&&(n.iridescence=this.iridescence),this.iridescenceIOR!==void 0&&(n.iridescenceIOR=this.iridescenceIOR),this.iridescenceThicknessRange!==void 0&&(n.iridescenceThicknessRange=this.iridescenceThicknessRange),this.iridescenceMap&&this.iridescenceMap.isTexture&&(n.iridescenceMap=this.iridescenceMap.toJSON(e).uuid),this.iridescenceThicknessMap&&this.iridescenceThicknessMap.isTexture&&(n.iridescenceThicknessMap=this.iridescenceThicknessMap.toJSON(e).uuid),this.anisotropy!==void 0&&(n.anisotropy=this.anisotropy),this.anisotropyRotation!==void 0&&(n.anisotropyRotation=this.anisotropyRotation),this.anisotropyMap&&this.anisotropyMap.isTexture&&(n.anisotropyMap=this.anisotropyMap.toJSON(e).uuid),this.map&&this.map.isTexture&&(n.map=this.map.toJSON(e).uuid),this.matcap&&this.matcap.isTexture&&(n.matcap=this.matcap.toJSON(e).uuid),this.alphaMap&&this.alphaMap.isTexture&&(n.alphaMap=this.alphaMap.toJSON(e).uuid),this.lightMap&&this.lightMap.isTexture&&(n.lightMap=this.lightMap.toJSON(e).uuid,n.lightMapIntensity=this.lightMapIntensity),this.aoMap&&this.aoMap.isTexture&&(n.aoMap=this.aoMap.toJSON(e).uuid,n.aoMapIntensity=this.aoMapIntensity),this.bumpMap&&this.bumpMap.isTexture&&(n.bumpMap=this.bumpMap.toJSON(e).uuid,n.bumpScale=this.bumpScale),this.normalMap&&this.normalMap.isTexture&&(n.normalMap=this.normalMap.toJSON(e).uuid,n.normalMapType=this.normalMapType,n.normalScale=this.normalScale.toArray()),this.displacementMap&&this.displacementMap.isTexture&&(n.displacementMap=this.displacementMap.toJSON(e).uuid,n.displacementScale=this.displacementScale,n.displacementBias=this.displacementBias),this.roughnessMap&&this.roughnessMap.isTexture&&(n.roughnessMap=this.roughnessMap.toJSON(e).uuid),this.metalnessMap&&this.metalnessMap.isTexture&&(n.metalnessMap=this.metalnessMap.toJSON(e).uuid),this.emissiveMap&&this.emissiveMap.isTexture&&(n.emissiveMap=this.emissiveMap.toJSON(e).uuid),this.specularMap&&this.specularMap.isTexture&&(n.specularMap=this.specularMap.toJSON(e).uuid),this.specularIntensityMap&&this.specularIntensityMap.isTexture&&(n.specularIntensityMap=this.specularIntensityMap.toJSON(e).uuid),this.specularColorMap&&this.specularColorMap.isTexture&&(n.specularColorMap=this.specularColorMap.toJSON(e).uuid),this.envMap&&this.envMap.isTexture&&(n.envMap=this.envMap.toJSON(e).uuid,this.combine!==void 0&&(n.combine=this.combine)),this.envMapRotation!==void 0&&(n.envMapRotation=this.envMapRotation.toArray()),this.envMapIntensity!==void 0&&(n.envMapIntensity=this.envMapIntensity),this.reflectivity!==void 0&&(n.reflectivity=this.reflectivity),this.refractionRatio!==void 0&&(n.refractionRatio=this.refractionRatio),this.gradientMap&&this.gradientMap.isTexture&&(n.gradientMap=this.gradientMap.toJSON(e).uuid),this.transmission!==void 0&&(n.transmission=this.transmission),this.transmissionMap&&this.transmissionMap.isTexture&&(n.transmissionMap=this.transmissionMap.toJSON(e).uuid),this.thickness!==void 0&&(n.thickness=this.thickness),this.thicknessMap&&this.thicknessMap.isTexture&&(n.thicknessMap=this.thicknessMap.toJSON(e).uuid),this.attenuationDistance!==void 0&&this.attenuationDistance!==1/0&&(n.attenuationDistance=this.attenuationDistance),this.attenuationColor!==void 0&&(n.attenuationColor=this.attenuationColor.getHex()),this.size!==void 0&&(n.size=this.size),this.shadowSide!==null&&(n.shadowSide=this.shadowSide),this.sizeAttenuation!==void 0&&(n.sizeAttenuation=this.sizeAttenuation),this.blending!==1&&(n.blending=this.blending),this.side!==0&&(n.side=this.side),this.vertexColors===!0&&(n.vertexColors=!0),this.opacity<1&&(n.opacity=this.opacity),this.transparent===!0&&(n.transparent=!0),this.blendSrc!==204&&(n.blendSrc=this.blendSrc),this.blendDst!==205&&(n.blendDst=this.blendDst),this.blendEquation!==100&&(n.blendEquation=this.blendEquation),this.blendSrcAlpha!==null&&(n.blendSrcAlpha=this.blendSrcAlpha),this.blendDstAlpha!==null&&(n.blendDstAlpha=this.blendDstAlpha),this.blendEquationAlpha!==null&&(n.blendEquationAlpha=this.blendEquationAlpha),this.blendColor&&this.blendColor.isColor&&(n.blendColor=this.blendColor.getHex()),this.blendAlpha!==0&&(n.blendAlpha=this.blendAlpha),this.depthFunc!==3&&(n.depthFunc=this.depthFunc),this.depthTest===!1&&(n.depthTest=this.depthTest),this.depthWrite===!1&&(n.depthWrite=this.depthWrite),this.colorWrite===!1&&(n.colorWrite=this.colorWrite),this.stencilWriteMask!==255&&(n.stencilWriteMask=this.stencilWriteMask),this.stencilFunc!==519&&(n.stencilFunc=this.stencilFunc),this.stencilRef!==0&&(n.stencilRef=this.stencilRef),this.stencilFuncMask!==255&&(n.stencilFuncMask=this.stencilFuncMask),this.stencilFail!==7680&&(n.stencilFail=this.stencilFail),this.stencilZFail!==7680&&(n.stencilZFail=this.stencilZFail),this.stencilZPass!==7680&&(n.stencilZPass=this.stencilZPass),this.stencilWrite===!0&&(n.stencilWrite=this.stencilWrite),this.rotation!==void 0&&this.rotation!==0&&(n.rotation=this.rotation),this.polygonOffset===!0&&(n.polygonOffset=!0),this.polygonOffsetFactor!==0&&(n.polygonOffsetFactor=this.polygonOffsetFactor),this.polygonOffsetUnits!==0&&(n.polygonOffsetUnits=this.polygonOffsetUnits),this.linewidth!==void 0&&this.linewidth!==1&&(n.linewidth=this.linewidth),this.dashSize!==void 0&&(n.dashSize=this.dashSize),this.gapSize!==void 0&&(n.gapSize=this.gapSize),this.scale!==void 0&&(n.scale=this.scale),this.dithering===!0&&(n.dithering=!0),this.alphaTest>0&&(n.alphaTest=this.alphaTest),this.alphaHash===!0&&(n.alphaHash=!0),this.alphaToCoverage===!0&&(n.alphaToCoverage=!0),this.premultipliedAlpha===!0&&(n.premultipliedAlpha=!0),this.forceSinglePass===!0&&(n.forceSinglePass=!0),this.wireframe===!0&&(n.wireframe=!0),this.wireframeLinewidth>1&&(n.wireframeLinewidth=this.wireframeLinewidth),this.wireframeLinecap!==`round`&&(n.wireframeLinecap=this.wireframeLinecap),this.wireframeLinejoin!==`round`&&(n.wireframeLinejoin=this.wireframeLinejoin),this.flatShading===!0&&(n.flatShading=!0),this.visible===!1&&(n.visible=!1),this.toneMapped===!1&&(n.toneMapped=!1),this.fog===!1&&(n.fog=!1),Object.keys(this.userData).length>0&&(n.userData=this.userData);function r(e){let t=[];for(let n in e){let r=e[n];delete r.metadata,t.push(r)}return t}if(t){let t=r(e.textures),i=r(e.images);t.length>0&&(n.textures=t),i.length>0&&(n.images=i)}return n}clone(){return new this.constructor().copy(this)}copy(e){this.name=e.name,this.blending=e.blending,this.side=e.side,this.vertexColors=e.vertexColors,this.opacity=e.opacity,this.transparent=e.transparent,this.blendSrc=e.blendSrc,this.blendDst=e.blendDst,this.blendEquation=e.blendEquation,this.blendSrcAlpha=e.blendSrcAlpha,this.blendDstAlpha=e.blendDstAlpha,this.blendEquationAlpha=e.blendEquationAlpha,this.blendColor.copy(e.blendColor),this.blendAlpha=e.blendAlpha,this.depthFunc=e.depthFunc,this.depthTest=e.depthTest,this.depthWrite=e.depthWrite,this.stencilWriteMask=e.stencilWriteMask,this.stencilFunc=e.stencilFunc,this.stencilRef=e.stencilRef,this.stencilFuncMask=e.stencilFuncMask,this.stencilFail=e.stencilFail,this.stencilZFail=e.stencilZFail,this.stencilZPass=e.stencilZPass,this.stencilWrite=e.stencilWrite;let t=e.clippingPlanes,n=null;if(t!==null){let e=t.length;n=Array(e);for(let r=0;r!==e;++r)n[r]=t[r].clone()}return this.clippingPlanes=n,this.clipIntersection=e.clipIntersection,this.clipShadows=e.clipShadows,this.shadowSide=e.shadowSide,this.colorWrite=e.colorWrite,this.precision=e.precision,this.polygonOffset=e.polygonOffset,this.polygonOffsetFactor=e.polygonOffsetFactor,this.polygonOffsetUnits=e.polygonOffsetUnits,this.dithering=e.dithering,this.alphaTest=e.alphaTest,this.alphaHash=e.alphaHash,this.alphaToCoverage=e.alphaToCoverage,this.premultipliedAlpha=e.premultipliedAlpha,this.forceSinglePass=e.forceSinglePass,this.visible=e.visible,this.toneMapped=e.toneMapped,this.userData=JSON.parse(JSON.stringify(e.userData)),this}dispose(){this.dispatchEvent({type:`dispose`})}set needsUpdate(e){e===!0&&this.version++}},Ea=class extends Ta{constructor(e){super(),this.isMeshBasicMaterial=!0,this.type=`MeshBasicMaterial`,this.color=new Sa(16777215),this.map=null,this.lightMap=null,this.lightMapIntensity=1,this.aoMap=null,this.aoMapIntensity=1,this.specularMap=null,this.alphaMap=null,this.envMap=null,this.envMapRotation=new Ui,this.combine=0,this.reflectivity=1,this.refractionRatio=.98,this.wireframe=!1,this.wireframeLinewidth=1,this.wireframeLinecap=`round`,this.wireframeLinejoin=`round`,this.fog=!0,this.setValues(e)}copy(e){return super.copy(e),this.color.copy(e.color),this.map=e.map,this.lightMap=e.lightMap,this.lightMapIntensity=e.lightMapIntensity,this.aoMap=e.aoMap,this.aoMapIntensity=e.aoMapIntensity,this.specularMap=e.specularMap,this.alphaMap=e.alphaMap,this.envMap=e.envMap,this.envMapRotation.copy(e.envMapRotation),this.combine=e.combine,this.reflectivity=e.reflectivity,this.refractionRatio=e.refractionRatio,this.wireframe=e.wireframe,this.wireframeLinewidth=e.wireframeLinewidth,this.wireframeLinecap=e.wireframeLinecap,this.wireframeLinejoin=e.wireframeLinejoin,this.fog=e.fog,this}},Da=new Z,Oa=new Y,ka=class{constructor(e,t,n=!1){if(Array.isArray(e))throw TypeError(`THREE.BufferAttribute: array should be a Typed Array.`);this.isBufferAttribute=!0,this.name=``,this.array=e,this.itemSize=t,this.count=e===void 0?0:e.length/t,this.normalized=n,this.usage=tr,this._updateRange={offset:0,count:-1},this.updateRanges=[],this.gpuType=Nn,this.version=0}onUploadCallback(){}set needsUpdate(e){e===!0&&this.version++}get updateRange(){return Lr(`THREE.BufferAttribute: updateRange() is deprecated and will be removed in r169. Use addUpdateRange() instead.`),this._updateRange}setUsage(e){return this.usage=e,this}addUpdateRange(e,t){this.updateRanges.push({start:e,count:t})}clearUpdateRanges(){this.updateRanges.length=0}copy(e){return this.name=e.name,this.array=new e.array.constructor(e.array),this.itemSize=e.itemSize,this.count=e.count,this.normalized=e.normalized,this.usage=e.usage,this.gpuType=e.gpuType,this}copyAt(e,t,n){e*=this.itemSize,n*=t.itemSize;for(let r=0,i=this.itemSize;r<i;r++)this.array[e+r]=t.array[n+r];return this}copyArray(e){return this.array.set(e),this}applyMatrix3(e){if(this.itemSize===2)for(let t=0,n=this.count;t<n;t++)Oa.fromBufferAttribute(this,t),Oa.applyMatrix3(e),this.setXY(t,Oa.x,Oa.y);else if(this.itemSize===3)for(let t=0,n=this.count;t<n;t++)Da.fromBufferAttribute(this,t),Da.applyMatrix3(e),this.setXYZ(t,Da.x,Da.y,Da.z);return this}applyMatrix4(e){for(let t=0,n=this.count;t<n;t++)Da.fromBufferAttribute(this,t),Da.applyMatrix4(e),this.setXYZ(t,Da.x,Da.y,Da.z);return this}applyNormalMatrix(e){for(let t=0,n=this.count;t<n;t++)Da.fromBufferAttribute(this,t),Da.applyNormalMatrix(e),this.setXYZ(t,Da.x,Da.y,Da.z);return this}transformDirection(e){for(let t=0,n=this.count;t<n;t++)Da.fromBufferAttribute(this,t),Da.transformDirection(e),this.setXYZ(t,Da.x,Da.y,Da.z);return this}set(e,t=0){return this.array.set(e,t),this}getComponent(e,t){let n=this.array[e*this.itemSize+t];return this.normalized&&(n=kr(n,this.array)),n}setComponent(e,t,n){return this.normalized&&(n=Ar(n,this.array)),this.array[e*this.itemSize+t]=n,this}getX(e){let t=this.array[e*this.itemSize];return this.normalized&&(t=kr(t,this.array)),t}setX(e,t){return this.normalized&&(t=Ar(t,this.array)),this.array[e*this.itemSize]=t,this}getY(e){let t=this.array[e*this.itemSize+1];return this.normalized&&(t=kr(t,this.array)),t}setY(e,t){return this.normalized&&(t=Ar(t,this.array)),this.array[e*this.itemSize+1]=t,this}getZ(e){let t=this.array[e*this.itemSize+2];return this.normalized&&(t=kr(t,this.array)),t}setZ(e,t){return this.normalized&&(t=Ar(t,this.array)),this.array[e*this.itemSize+2]=t,this}getW(e){let t=this.array[e*this.itemSize+3];return this.normalized&&(t=kr(t,this.array)),t}setW(e,t){return this.normalized&&(t=Ar(t,this.array)),this.array[e*this.itemSize+3]=t,this}setXY(e,t,n){return e*=this.itemSize,this.normalized&&(t=Ar(t,this.array),n=Ar(n,this.array)),this.array[e+0]=t,this.array[e+1]=n,this}setXYZ(e,t,n,r){return e*=this.itemSize,this.normalized&&(t=Ar(t,this.array),n=Ar(n,this.array),r=Ar(r,this.array)),this.array[e+0]=t,this.array[e+1]=n,this.array[e+2]=r,this}setXYZW(e,t,n,r,i){return e*=this.itemSize,this.normalized&&(t=Ar(t,this.array),n=Ar(n,this.array),r=Ar(r,this.array),i=Ar(i,this.array)),this.array[e+0]=t,this.array[e+1]=n,this.array[e+2]=r,this.array[e+3]=i,this}onUpload(e){return this.onUploadCallback=e,this}clone(){return new this.constructor(this.array,this.itemSize).copy(this)}toJSON(){let e={itemSize:this.itemSize,type:this.array.constructor.name,array:Array.from(this.array),normalized:this.normalized};return this.name!==``&&(e.name=this.name),this.usage!==35044&&(e.usage=this.usage),e}},Aa=class extends ka{constructor(e,t,n){super(new Uint16Array(e),t,n)}},ja=class extends ka{constructor(e,t,n){super(new Uint32Array(e),t,n)}},Ma=class extends ka{constructor(e,t,n){super(new Float32Array(e),t,n)}},Na=0,Pa=new Ni,Fa=new oa,Ia=new Z,La=new oi,Ra=new oi,za=new Z,Ba=class e extends ir{constructor(){super(),this.isBufferGeometry=!0,Object.defineProperty(this,`id`,{value:Na++}),this.uuid=lr(),this.name=``,this.type=`BufferGeometry`,this.index=null,this.attributes={},this.morphAttributes={},this.morphTargetsRelative=!1,this.groups=[],this.boundingBox=null,this.boundingSphere=null,this.drawRange={start:0,count:1/0},this.userData={}}getIndex(){return this.index}setIndex(e){return Array.isArray(e)?this.index=new(Nr(e)?ja:Aa)(e,1):this.index=e,this}getAttribute(e){return this.attributes[e]}setAttribute(e,t){return this.attributes[e]=t,this}deleteAttribute(e){return delete this.attributes[e],this}hasAttribute(e){return this.attributes[e]!==void 0}addGroup(e,t,n=0){this.groups.push({start:e,count:t,materialIndex:n})}clearGroups(){this.groups=[]}setDrawRange(e,t){this.drawRange.start=e,this.drawRange.count=t}applyMatrix4(e){let t=this.attributes.position;t!==void 0&&(t.applyMatrix4(e),t.needsUpdate=!0);let n=this.attributes.normal;if(n!==void 0){let t=new X().getNormalMatrix(e);n.applyNormalMatrix(t),n.needsUpdate=!0}let r=this.attributes.tangent;return r!==void 0&&(r.transformDirection(e),r.needsUpdate=!0),this.boundingBox!==null&&this.computeBoundingBox(),this.boundingSphere!==null&&this.computeBoundingSphere(),this}applyQuaternion(e){return Pa.makeRotationFromQuaternion(e),this.applyMatrix4(Pa),this}rotateX(e){return Pa.makeRotationX(e),this.applyMatrix4(Pa),this}rotateY(e){return Pa.makeRotationY(e),this.applyMatrix4(Pa),this}rotateZ(e){return Pa.makeRotationZ(e),this.applyMatrix4(Pa),this}translate(e,t,n){return Pa.makeTranslation(e,t,n),this.applyMatrix4(Pa),this}scale(e,t,n){return Pa.makeScale(e,t,n),this.applyMatrix4(Pa),this}lookAt(e){return Fa.lookAt(e),Fa.updateMatrix(),this.applyMatrix4(Fa.matrix),this}center(){return this.computeBoundingBox(),this.boundingBox.getCenter(Ia).negate(),this.translate(Ia.x,Ia.y,Ia.z),this}setFromPoints(e){let t=[];for(let n=0,r=e.length;n<r;n++){let r=e[n];t.push(r.x,r.y,r.z||0)}return this.setAttribute(`position`,new Ma(t,3)),this}computeBoundingBox(){this.boundingBox===null&&(this.boundingBox=new oi);let e=this.attributes.position,t=this.morphAttributes.position;if(e&&e.isGLBufferAttribute){console.error(`THREE.BufferGeometry.computeBoundingBox(): GLBufferAttribute requires a manual bounding box.`,this),this.boundingBox.set(new Z(-1/0,-1/0,-1/0),new Z(1/0,1/0,1/0));return}if(e!==void 0){if(this.boundingBox.setFromBufferAttribute(e),t)for(let e=0,n=t.length;e<n;e++){let n=t[e];La.setFromBufferAttribute(n),this.morphTargetsRelative?(za.addVectors(this.boundingBox.min,La.min),this.boundingBox.expandByPoint(za),za.addVectors(this.boundingBox.max,La.max),this.boundingBox.expandByPoint(za)):(this.boundingBox.expandByPoint(La.min),this.boundingBox.expandByPoint(La.max))}}else this.boundingBox.makeEmpty();(isNaN(this.boundingBox.min.x)||isNaN(this.boundingBox.min.y)||isNaN(this.boundingBox.min.z))&&console.error(`THREE.BufferGeometry.computeBoundingBox(): Computed min/max have NaN values. The "position" attribute is likely to have NaN values.`,this)}computeBoundingSphere(){this.boundingSphere===null&&(this.boundingSphere=new wi);let e=this.attributes.position,t=this.morphAttributes.position;if(e&&e.isGLBufferAttribute){console.error(`THREE.BufferGeometry.computeBoundingSphere(): GLBufferAttribute requires a manual bounding sphere.`,this),this.boundingSphere.set(new Z,1/0);return}if(e){let n=this.boundingSphere.center;if(La.setFromBufferAttribute(e),t)for(let e=0,n=t.length;e<n;e++){let n=t[e];Ra.setFromBufferAttribute(n),this.morphTargetsRelative?(za.addVectors(La.min,Ra.min),La.expandByPoint(za),za.addVectors(La.max,Ra.max),La.expandByPoint(za)):(La.expandByPoint(Ra.min),La.expandByPoint(Ra.max))}La.getCenter(n);let r=0;for(let t=0,i=e.count;t<i;t++)za.fromBufferAttribute(e,t),r=Math.max(r,n.distanceToSquared(za));if(t)for(let i=0,a=t.length;i<a;i++){let a=t[i],o=this.morphTargetsRelative;for(let t=0,i=a.count;t<i;t++)za.fromBufferAttribute(a,t),o&&(Ia.fromBufferAttribute(e,t),za.add(Ia)),r=Math.max(r,n.distanceToSquared(za))}this.boundingSphere.radius=Math.sqrt(r),isNaN(this.boundingSphere.radius)&&console.error(`THREE.BufferGeometry.computeBoundingSphere(): Computed radius is NaN. The "position" attribute is likely to have NaN values.`,this)}}computeTangents(){let e=this.index,t=this.attributes;if(e===null||t.position===void 0||t.normal===void 0||t.uv===void 0){console.error(`THREE.BufferGeometry: .computeTangents() failed. Missing required attributes (index, position, normal or uv)`);return}let n=t.position,r=t.normal,i=t.uv;this.hasAttribute(`tangent`)===!1&&this.setAttribute(`tangent`,new ka(new Float32Array(4*n.count),4));let a=this.getAttribute(`tangent`),o=[],s=[];for(let e=0;e<n.count;e++)o[e]=new Z,s[e]=new Z;let c=new Z,l=new Z,u=new Z,d=new Y,f=new Y,p=new Y,m=new Z,h=new Z;function g(e,t,r){c.fromBufferAttribute(n,e),l.fromBufferAttribute(n,t),u.fromBufferAttribute(n,r),d.fromBufferAttribute(i,e),f.fromBufferAttribute(i,t),p.fromBufferAttribute(i,r),l.sub(c),u.sub(c),f.sub(d),p.sub(d);let a=1/(f.x*p.y-p.x*f.y);isFinite(a)&&(m.copy(l).multiplyScalar(p.y).addScaledVector(u,-f.y).multiplyScalar(a),h.copy(u).multiplyScalar(f.x).addScaledVector(l,-p.x).multiplyScalar(a),o[e].add(m),o[t].add(m),o[r].add(m),s[e].add(h),s[t].add(h),s[r].add(h))}let _=this.groups;_.length===0&&(_=[{start:0,count:e.count}]);for(let t=0,n=_.length;t<n;++t){let n=_[t],r=n.start,i=n.count;for(let t=r,n=r+i;t<n;t+=3)g(e.getX(t+0),e.getX(t+1),e.getX(t+2))}let v=new Z,y=new Z,b=new Z,x=new Z;function S(e){b.fromBufferAttribute(r,e),x.copy(b);let t=o[e];v.copy(t),v.sub(b.multiplyScalar(b.dot(t))).normalize(),y.crossVectors(x,t);let n=y.dot(s[e])<0?-1:1;a.setXYZW(e,v.x,v.y,v.z,n)}for(let t=0,n=_.length;t<n;++t){let n=_[t],r=n.start,i=n.count;for(let t=r,n=r+i;t<n;t+=3)S(e.getX(t+0)),S(e.getX(t+1)),S(e.getX(t+2))}}computeVertexNormals(){let e=this.index,t=this.getAttribute(`position`);if(t!==void 0){let n=this.getAttribute(`normal`);if(n===void 0)n=new ka(new Float32Array(t.count*3),3),this.setAttribute(`normal`,n);else for(let e=0,t=n.count;e<t;e++)n.setXYZ(e,0,0,0);let r=new Z,i=new Z,a=new Z,o=new Z,s=new Z,c=new Z,l=new Z,u=new Z;if(e)for(let d=0,f=e.count;d<f;d+=3){let f=e.getX(d+0),p=e.getX(d+1),m=e.getX(d+2);r.fromBufferAttribute(t,f),i.fromBufferAttribute(t,p),a.fromBufferAttribute(t,m),l.subVectors(a,i),u.subVectors(r,i),l.cross(u),o.fromBufferAttribute(n,f),s.fromBufferAttribute(n,p),c.fromBufferAttribute(n,m),o.add(l),s.add(l),c.add(l),n.setXYZ(f,o.x,o.y,o.z),n.setXYZ(p,s.x,s.y,s.z),n.setXYZ(m,c.x,c.y,c.z)}else for(let e=0,o=t.count;e<o;e+=3)r.fromBufferAttribute(t,e+0),i.fromBufferAttribute(t,e+1),a.fromBufferAttribute(t,e+2),l.subVectors(a,i),u.subVectors(r,i),l.cross(u),n.setXYZ(e+0,l.x,l.y,l.z),n.setXYZ(e+1,l.x,l.y,l.z),n.setXYZ(e+2,l.x,l.y,l.z);this.normalizeNormals(),n.needsUpdate=!0}}normalizeNormals(){let e=this.attributes.normal;for(let t=0,n=e.count;t<n;t++)za.fromBufferAttribute(e,t),za.normalize(),e.setXYZ(t,za.x,za.y,za.z)}toNonIndexed(){function t(e,t){let n=e.array,r=e.itemSize,i=e.normalized,a=new n.constructor(t.length*r),o=0,s=0;for(let i=0,c=t.length;i<c;i++){o=e.isInterleavedBufferAttribute?t[i]*e.data.stride+e.offset:t[i]*r;for(let e=0;e<r;e++)a[s++]=n[o++]}return new ka(a,r,i)}if(this.index===null)return console.warn(`THREE.BufferGeometry.toNonIndexed(): BufferGeometry is already non-indexed.`),this;let n=new e,r=this.index.array,i=this.attributes;for(let e in i){let a=i[e],o=t(a,r);n.setAttribute(e,o)}let a=this.morphAttributes;for(let e in a){let i=[],o=a[e];for(let e=0,n=o.length;e<n;e++){let n=o[e],a=t(n,r);i.push(a)}n.morphAttributes[e]=i}n.morphTargetsRelative=this.morphTargetsRelative;let o=this.groups;for(let e=0,t=o.length;e<t;e++){let t=o[e];n.addGroup(t.start,t.count,t.materialIndex)}return n}toJSON(){let e={metadata:{version:4.6,type:`BufferGeometry`,generator:`BufferGeometry.toJSON`}};if(e.uuid=this.uuid,e.type=this.type,this.name!==``&&(e.name=this.name),Object.keys(this.userData).length>0&&(e.userData=this.userData),this.parameters!==void 0){let t=this.parameters;for(let n in t)t[n]!==void 0&&(e[n]=t[n]);return e}e.data={attributes:{}};let t=this.index;t!==null&&(e.data.index={type:t.array.constructor.name,array:Array.prototype.slice.call(t.array)});let n=this.attributes;for(let t in n){let r=n[t];e.data.attributes[t]=r.toJSON(e.data)}let r={},i=!1;for(let t in this.morphAttributes){let n=this.morphAttributes[t],a=[];for(let t=0,r=n.length;t<r;t++){let r=n[t];a.push(r.toJSON(e.data))}a.length>0&&(r[t]=a,i=!0)}i&&(e.data.morphAttributes=r,e.data.morphTargetsRelative=this.morphTargetsRelative);let a=this.groups;a.length>0&&(e.data.groups=JSON.parse(JSON.stringify(a)));let o=this.boundingSphere;return o!==null&&(e.data.boundingSphere={center:o.center.toArray(),radius:o.radius}),e}clone(){return new this.constructor().copy(this)}copy(e){this.index=null,this.attributes={},this.morphAttributes={},this.groups=[],this.boundingBox=null,this.boundingSphere=null;let t={};this.name=e.name;let n=e.index;n!==null&&this.setIndex(n.clone(t));let r=e.attributes;for(let e in r){let n=r[e];this.setAttribute(e,n.clone(t))}let i=e.morphAttributes;for(let e in i){let n=[],r=i[e];for(let e=0,i=r.length;e<i;e++)n.push(r[e].clone(t));this.morphAttributes[e]=n}this.morphTargetsRelative=e.morphTargetsRelative;let a=e.groups;for(let e=0,t=a.length;e<t;e++){let t=a[e];this.addGroup(t.start,t.count,t.materialIndex)}let o=e.boundingBox;o!==null&&(this.boundingBox=o.clone());let s=e.boundingSphere;return s!==null&&(this.boundingSphere=s.clone()),this.drawRange.start=e.drawRange.start,this.drawRange.count=e.drawRange.count,this.userData=e.userData,this}dispose(){this.dispatchEvent({type:`dispose`})}},Va=new Ni,Ha=new Mi,Ua=new wi,Wa=new Z,Ga=new Z,Ka=new Z,qa=new Z,Ja=new Z,Ya=new Z,Xa=new Y,Za=new Y,Qa=new Y,$a=new Z,eo=new Z,to=new Z,no=new Z,ro=new Z,io=class extends oa{constructor(e=new Ba,t=new Ea){super(),this.isMesh=!0,this.type=`Mesh`,this.geometry=e,this.material=t,this.updateMorphTargets()}copy(e,t){return super.copy(e,t),e.morphTargetInfluences!==void 0&&(this.morphTargetInfluences=e.morphTargetInfluences.slice()),e.morphTargetDictionary!==void 0&&(this.morphTargetDictionary=Object.assign({},e.morphTargetDictionary)),this.material=Array.isArray(e.material)?e.material.slice():e.material,this.geometry=e.geometry,this}updateMorphTargets(){let e=this.geometry.morphAttributes,t=Object.keys(e);if(t.length>0){let n=e[t[0]];if(n!==void 0){this.morphTargetInfluences=[],this.morphTargetDictionary={};for(let e=0,t=n.length;e<t;e++){let t=n[e].name||String(e);this.morphTargetInfluences.push(0),this.morphTargetDictionary[t]=e}}}}getVertexPosition(e,t){let n=this.geometry,r=n.attributes.position,i=n.morphAttributes.position,a=n.morphTargetsRelative;t.fromBufferAttribute(r,e);let o=this.morphTargetInfluences;if(i&&o){Ya.set(0,0,0);for(let n=0,r=i.length;n<r;n++){let r=o[n],s=i[n];r!==0&&(Ja.fromBufferAttribute(s,e),a?Ya.addScaledVector(Ja,r):Ya.addScaledVector(Ja.sub(t),r))}t.add(Ya)}return t}raycast(e,t){let n=this.geometry,r=this.material,i=this.matrixWorld;r!==void 0&&(n.boundingSphere===null&&n.computeBoundingSphere(),Ua.copy(n.boundingSphere),Ua.applyMatrix4(i),Ha.copy(e.ray).recast(e.near),!(Ua.containsPoint(Ha.origin)===!1&&(Ha.intersectSphere(Ua,Wa)===null||Ha.origin.distanceToSquared(Wa)>(e.far-e.near)**2))&&(Va.copy(i).invert(),Ha.copy(e.ray).applyMatrix4(Va),!(n.boundingBox!==null&&Ha.intersectsBox(n.boundingBox)===!1)&&this._computeIntersections(e,t,Ha)))}_computeIntersections(e,t,n){let r,i=this.geometry,a=this.material,o=i.index,s=i.attributes.position,c=i.attributes.uv,l=i.attributes.uv1,u=i.attributes.normal,d=i.groups,f=i.drawRange;if(o!==null)if(Array.isArray(a))for(let i=0,s=d.length;i<s;i++){let s=d[i],p=a[s.materialIndex],m=Math.max(s.start,f.start),h=Math.min(o.count,Math.min(s.start+s.count,f.start+f.count));for(let i=m,a=h;i<a;i+=3){let a=o.getX(i),d=o.getX(i+1),f=o.getX(i+2);r=oo(this,p,e,n,c,l,u,a,d,f),r&&(r.faceIndex=Math.floor(i/3),r.face.materialIndex=s.materialIndex,t.push(r))}}else{let i=Math.max(0,f.start),s=Math.min(o.count,f.start+f.count);for(let d=i,f=s;d<f;d+=3){let i=o.getX(d),s=o.getX(d+1),f=o.getX(d+2);r=oo(this,a,e,n,c,l,u,i,s,f),r&&(r.faceIndex=Math.floor(d/3),t.push(r))}}else if(s!==void 0)if(Array.isArray(a))for(let i=0,o=d.length;i<o;i++){let o=d[i],p=a[o.materialIndex],m=Math.max(o.start,f.start),h=Math.min(s.count,Math.min(o.start+o.count,f.start+f.count));for(let i=m,a=h;i<a;i+=3){let a=i,s=i+1,d=i+2;r=oo(this,p,e,n,c,l,u,a,s,d),r&&(r.faceIndex=Math.floor(i/3),r.face.materialIndex=o.materialIndex,t.push(r))}}else{let i=Math.max(0,f.start),o=Math.min(s.count,f.start+f.count);for(let s=i,d=o;s<d;s+=3){let i=s,o=s+1,d=s+2;r=oo(this,a,e,n,c,l,u,i,o,d),r&&(r.faceIndex=Math.floor(s/3),t.push(r))}}}};function ao(e,t,n,r,i,a,o,s){let c;if(c=t.side===1?r.intersectTriangle(o,a,i,!0,s):r.intersectTriangle(i,a,o,t.side===0,s),c===null)return null;ro.copy(s),ro.applyMatrix4(e.matrixWorld);let l=n.ray.origin.distanceTo(ro);return l<n.near||l>n.far?null:{distance:l,point:ro.clone(),object:e}}function oo(e,t,n,r,i,a,o,s,c,l){e.getVertexPosition(s,Ga),e.getVertexPosition(c,Ka),e.getVertexPosition(l,qa);let u=ao(e,t,n,r,Ga,Ka,qa,no);if(u){i&&(Xa.fromBufferAttribute(i,s),Za.fromBufferAttribute(i,c),Qa.fromBufferAttribute(i,l),u.uv=_a.getInterpolation(no,Ga,Ka,qa,Xa,Za,Qa,new Y)),a&&(Xa.fromBufferAttribute(a,s),Za.fromBufferAttribute(a,c),Qa.fromBufferAttribute(a,l),u.uv1=_a.getInterpolation(no,Ga,Ka,qa,Xa,Za,Qa,new Y)),o&&($a.fromBufferAttribute(o,s),eo.fromBufferAttribute(o,c),to.fromBufferAttribute(o,l),u.normal=_a.getInterpolation(no,Ga,Ka,qa,$a,eo,to,new Z),u.normal.dot(r.direction)>0&&u.normal.multiplyScalar(-1));let e={a:s,b:c,c:l,normal:new Z,materialIndex:0};_a.getNormal(Ga,Ka,qa,e.normal),u.face=e}return u}var so=class e extends Ba{constructor(e=1,t=1,n=1,r=1,i=1,a=1){super(),this.type=`BoxGeometry`,this.parameters={width:e,height:t,depth:n,widthSegments:r,heightSegments:i,depthSegments:a};let o=this;r=Math.floor(r),i=Math.floor(i),a=Math.floor(a);let s=[],c=[],l=[],u=[],d=0,f=0;p(`z`,`y`,`x`,-1,-1,n,t,e,a,i,0),p(`z`,`y`,`x`,1,-1,n,t,-e,a,i,1),p(`x`,`z`,`y`,1,1,e,n,t,r,a,2),p(`x`,`z`,`y`,1,-1,e,n,-t,r,a,3),p(`x`,`y`,`z`,1,-1,e,t,n,r,i,4),p(`x`,`y`,`z`,-1,-1,e,t,-n,r,i,5),this.setIndex(s),this.setAttribute(`position`,new Ma(c,3)),this.setAttribute(`normal`,new Ma(l,3)),this.setAttribute(`uv`,new Ma(u,2));function p(e,t,n,r,i,a,p,m,h,g,_){let v=a/h,y=p/g,b=a/2,x=p/2,S=m/2,C=h+1,w=g+1,T=0,E=0,D=new Z;for(let a=0;a<w;a++){let o=a*y-x;for(let s=0;s<C;s++)D[e]=(s*v-b)*r,D[t]=o*i,D[n]=S,c.push(D.x,D.y,D.z),D[e]=0,D[t]=0,D[n]=m>0?1:-1,l.push(D.x,D.y,D.z),u.push(s/h),u.push(1-a/g),T+=1}for(let e=0;e<g;e++)for(let t=0;t<h;t++){let n=d+t+C*e,r=d+t+C*(e+1),i=d+(t+1)+C*(e+1),a=d+(t+1)+C*e;s.push(n,r,a),s.push(r,i,a),E+=6}o.addGroup(f,E,_),f+=E,d+=T}}copy(e){return super.copy(e),this.parameters=Object.assign({},e.parameters),this}static fromJSON(t){return new e(t.width,t.height,t.depth,t.widthSegments,t.heightSegments,t.depthSegments)}};function co(e){let t={};for(let n in e){t[n]={};for(let r in e[n]){let i=e[n][r];i&&(i.isColor||i.isMatrix3||i.isMatrix4||i.isVector2||i.isVector3||i.isVector4||i.isTexture||i.isQuaternion)?i.isRenderTargetTexture?(console.warn(`UniformsUtils: Textures of render targets cannot be cloned via cloneUniforms() or mergeUniforms().`),t[n][r]=null):t[n][r]=i.clone():Array.isArray(i)?t[n][r]=i.slice():t[n][r]=i}}return t}function lo(e){let t={};for(let n=0;n<e.length;n++){let r=co(e[n]);for(let e in r)t[e]=r[e]}return t}function uo(e){let t=[];for(let n=0;n<e.length;n++)t.push(e[n].clone());return t}function fo(e){return e.getRenderTarget()===null?e.outputColorSpace:Hr.workingColorSpace}var po={clone:co,merge:lo},mo=`void main() {
	gl_Position = projectionMatrix * modelViewMatrix * vec4( position, 1.0 );
}`,ho=`void main() {
	gl_FragColor = vec4( 1.0, 0.0, 0.0, 1.0 );
}`,go=class extends Ta{constructor(e){super(),this.isShaderMaterial=!0,this.type=`ShaderMaterial`,this.defines={},this.uniforms={},this.uniformsGroups=[],this.vertexShader=mo,this.fragmentShader=ho,this.linewidth=1,this.wireframe=!1,this.wireframeLinewidth=1,this.fog=!1,this.lights=!1,this.clipping=!1,this.forceSinglePass=!0,this.extensions={derivatives:!1,fragDepth:!1,drawBuffers:!1,shaderTextureLOD:!1,clipCullDistance:!1,multiDraw:!1},this.defaultAttributeValues={color:[1,1,1],uv:[0,0],uv1:[0,0]},this.index0AttributeName=void 0,this.uniformsNeedUpdate=!1,this.glslVersion=null,e!==void 0&&this.setValues(e)}copy(e){return super.copy(e),this.fragmentShader=e.fragmentShader,this.vertexShader=e.vertexShader,this.uniforms=co(e.uniforms),this.uniformsGroups=uo(e.uniformsGroups),this.defines=Object.assign({},e.defines),this.wireframe=e.wireframe,this.wireframeLinewidth=e.wireframeLinewidth,this.fog=e.fog,this.lights=e.lights,this.clipping=e.clipping,this.extensions=Object.assign({},e.extensions),this.glslVersion=e.glslVersion,this}toJSON(e){let t=super.toJSON(e);t.glslVersion=this.glslVersion,t.uniforms={};for(let n in this.uniforms){let r=this.uniforms[n].value;r&&r.isTexture?t.uniforms[n]={type:`t`,value:r.toJSON(e).uuid}:r&&r.isColor?t.uniforms[n]={type:`c`,value:r.getHex()}:r&&r.isVector2?t.uniforms[n]={type:`v2`,value:r.toArray()}:r&&r.isVector3?t.uniforms[n]={type:`v3`,value:r.toArray()}:r&&r.isVector4?t.uniforms[n]={type:`v4`,value:r.toArray()}:r&&r.isMatrix3?t.uniforms[n]={type:`m3`,value:r.toArray()}:r&&r.isMatrix4?t.uniforms[n]={type:`m4`,value:r.toArray()}:t.uniforms[n]={value:r}}Object.keys(this.defines).length>0&&(t.defines=this.defines),t.vertexShader=this.vertexShader,t.fragmentShader=this.fragmentShader,t.lights=this.lights,t.clipping=this.clipping;let n={};for(let e in this.extensions)this.extensions[e]===!0&&(n[e]=!0);return Object.keys(n).length>0&&(t.extensions=n),t}},_o=class extends oa{constructor(){super(),this.isCamera=!0,this.type=`Camera`,this.matrixWorldInverse=new Ni,this.projectionMatrix=new Ni,this.projectionMatrixInverse=new Ni,this.coordinateSystem=rr}copy(e,t){return super.copy(e,t),this.matrixWorldInverse.copy(e.matrixWorldInverse),this.projectionMatrix.copy(e.projectionMatrix),this.projectionMatrixInverse.copy(e.projectionMatrixInverse),this.coordinateSystem=e.coordinateSystem,this}getWorldDirection(e){return super.getWorldDirection(e).negate()}updateMatrixWorld(e){super.updateMatrixWorld(e),this.matrixWorldInverse.copy(this.matrixWorld).invert()}updateWorldMatrix(e,t){super.updateWorldMatrix(e,t),this.matrixWorldInverse.copy(this.matrixWorld).invert()}clone(){return new this.constructor().copy(this)}},vo=new Z,yo=new Y,bo=new Y,xo=class extends _o{constructor(e=50,t=1,n=.1,r=2e3){super(),this.isPerspectiveCamera=!0,this.type=`PerspectiveCamera`,this.fov=e,this.zoom=1,this.near=n,this.far=r,this.focus=10,this.aspect=t,this.view=null,this.filmGauge=35,this.filmOffset=0,this.updateProjectionMatrix()}copy(e,t){return super.copy(e,t),this.fov=e.fov,this.zoom=e.zoom,this.near=e.near,this.far=e.far,this.focus=e.focus,this.aspect=e.aspect,this.view=e.view===null?null:Object.assign({},e.view),this.filmGauge=e.filmGauge,this.filmOffset=e.filmOffset,this}setFocalLength(e){let t=.5*this.getFilmHeight()/e;this.fov=cr*2*Math.atan(t),this.updateProjectionMatrix()}getFocalLength(){let e=Math.tan(sr*.5*this.fov);return .5*this.getFilmHeight()/e}getEffectiveFOV(){return cr*2*Math.atan(Math.tan(sr*.5*this.fov)/this.zoom)}getFilmWidth(){return this.filmGauge*Math.min(this.aspect,1)}getFilmHeight(){return this.filmGauge/Math.max(this.aspect,1)}getViewBounds(e,t,n){vo.set(-1,-1,.5).applyMatrix4(this.projectionMatrixInverse),t.set(vo.x,vo.y).multiplyScalar(-e/vo.z),vo.set(1,1,.5).applyMatrix4(this.projectionMatrixInverse),n.set(vo.x,vo.y).multiplyScalar(-e/vo.z)}getViewSize(e,t){return this.getViewBounds(e,yo,bo),t.subVectors(bo,yo)}setViewOffset(e,t,n,r,i,a){this.aspect=e/t,this.view===null&&(this.view={enabled:!0,fullWidth:1,fullHeight:1,offsetX:0,offsetY:0,width:1,height:1}),this.view.enabled=!0,this.view.fullWidth=e,this.view.fullHeight=t,this.view.offsetX=n,this.view.offsetY=r,this.view.width=i,this.view.height=a,this.updateProjectionMatrix()}clearViewOffset(){this.view!==null&&(this.view.enabled=!1),this.updateProjectionMatrix()}updateProjectionMatrix(){let e=this.near,t=e*Math.tan(sr*.5*this.fov)/this.zoom,n=2*t,r=this.aspect*n,i=-.5*r,a=this.view;if(this.view!==null&&this.view.enabled){let e=a.fullWidth,o=a.fullHeight;i+=a.offsetX*r/e,t-=a.offsetY*n/o,r*=a.width/e,n*=a.height/o}let o=this.filmOffset;o!==0&&(i+=e*o/this.getFilmWidth()),this.projectionMatrix.makePerspective(i,i+r,t,t-n,e,this.far,this.coordinateSystem),this.projectionMatrixInverse.copy(this.projectionMatrix).invert()}toJSON(e){let t=super.toJSON(e);return t.object.fov=this.fov,t.object.zoom=this.zoom,t.object.near=this.near,t.object.far=this.far,t.object.focus=this.focus,t.object.aspect=this.aspect,this.view!==null&&(t.object.view=Object.assign({},this.view)),t.object.filmGauge=this.filmGauge,t.object.filmOffset=this.filmOffset,t}},So=-90,Co=1,wo=class extends oa{constructor(e,t,n){super(),this.type=`CubeCamera`,this.renderTarget=n,this.coordinateSystem=null,this.activeMipmapLevel=0;let r=new xo(So,Co,e,t);r.layers=this.layers,this.add(r);let i=new xo(So,Co,e,t);i.layers=this.layers,this.add(i);let a=new xo(So,Co,e,t);a.layers=this.layers,this.add(a);let o=new xo(So,Co,e,t);o.layers=this.layers,this.add(o);let s=new xo(So,Co,e,t);s.layers=this.layers,this.add(s);let c=new xo(So,Co,e,t);c.layers=this.layers,this.add(c)}updateCoordinateSystem(){let e=this.coordinateSystem,t=this.children.concat(),[n,r,i,a,o,s]=t;for(let e of t)this.remove(e);if(e===2e3)n.up.set(0,1,0),n.lookAt(1,0,0),r.up.set(0,1,0),r.lookAt(-1,0,0),i.up.set(0,0,-1),i.lookAt(0,1,0),a.up.set(0,0,1),a.lookAt(0,-1,0),o.up.set(0,1,0),o.lookAt(0,0,1),s.up.set(0,1,0),s.lookAt(0,0,-1);else if(e===2001)n.up.set(0,-1,0),n.lookAt(-1,0,0),r.up.set(0,-1,0),r.lookAt(1,0,0),i.up.set(0,0,1),i.lookAt(0,1,0),a.up.set(0,0,-1),a.lookAt(0,-1,0),o.up.set(0,-1,0),o.lookAt(0,0,1),s.up.set(0,-1,0),s.lookAt(0,0,-1);else throw Error(`THREE.CubeCamera.updateCoordinateSystem(): Invalid coordinate system: `+e);for(let e of t)this.add(e),e.updateMatrixWorld()}update(e,t){this.parent===null&&this.updateMatrixWorld();let{renderTarget:n,activeMipmapLevel:r}=this;this.coordinateSystem!==e.coordinateSystem&&(this.coordinateSystem=e.coordinateSystem,this.updateCoordinateSystem());let[i,a,o,s,c,l]=this.children,u=e.getRenderTarget(),d=e.getActiveCubeFace(),f=e.getActiveMipmapLevel(),p=e.xr.enabled;e.xr.enabled=!1;let m=n.texture.generateMipmaps;n.texture.generateMipmaps=!1,e.setRenderTarget(n,0,r),e.render(t,i),e.setRenderTarget(n,1,r),e.render(t,a),e.setRenderTarget(n,2,r),e.render(t,o),e.setRenderTarget(n,3,r),e.render(t,s),e.setRenderTarget(n,4,r),e.render(t,c),n.texture.generateMipmaps=m,e.setRenderTarget(n,5,r),e.render(t,l),e.setRenderTarget(u,d,f),e.xr.enabled=p,n.texture.needsPMREMUpdate=!0}},To=class extends Zr{constructor(e,t,n,r,i,a,o,s,c,l){e=e===void 0?[]:e,t=t===void 0?301:t,super(e,t,n,r,i,a,o,s,c,l),this.isCubeTexture=!0,this.flipY=!1}get images(){return this.image}set images(e){this.image=e}},Eo=class extends ei{constructor(e=1,t={}){super(e,e,t),this.isWebGLCubeRenderTarget=!0;let n={width:e,height:e,depth:1};this.texture=new To([n,n,n,n,n,n],t.mapping,t.wrapS,t.wrapT,t.magFilter,t.minFilter,t.format,t.type,t.anisotropy,t.colorSpace),this.texture.isRenderTargetTexture=!0,this.texture.generateMipmaps=t.generateMipmaps===void 0?!1:t.generateMipmaps,this.texture.minFilter=t.minFilter===void 0?On:t.minFilter}fromEquirectangularTexture(e,t){this.texture.type=t.type,this.texture.colorSpace=t.colorSpace,this.texture.generateMipmaps=t.generateMipmaps,this.texture.minFilter=t.minFilter,this.texture.magFilter=t.magFilter;let n={uniforms:{tEquirect:{value:null}},vertexShader:`

				varying vec3 vWorldDirection;

				vec3 transformDirection( in vec3 dir, in mat4 matrix ) {

					return normalize( ( matrix * vec4( dir, 0.0 ) ).xyz );

				}

				void main() {

					vWorldDirection = transformDirection( position, modelMatrix );

					#include <begin_vertex>
					#include <project_vertex>

				}
			`,fragmentShader:`

				uniform sampler2D tEquirect;

				varying vec3 vWorldDirection;

				#include <common>

				void main() {

					vec3 direction = normalize( vWorldDirection );

					vec2 sampleUV = equirectUv( direction );

					gl_FragColor = texture2D( tEquirect, sampleUV );

				}
			`},r=new so(5,5,5),i=new go({name:`CubemapFromEquirect`,uniforms:co(n.uniforms),vertexShader:n.vertexShader,fragmentShader:n.fragmentShader,side:1,blending:0});i.uniforms.tEquirect.value=t;let a=new io(r,i),o=t.minFilter;return t.minFilter===1008&&(t.minFilter=On),new wo(1,10,this).update(e,a),t.minFilter=o,a.geometry.dispose(),a.material.dispose(),this}clear(e,t,n,r){let i=e.getRenderTarget();for(let i=0;i<6;i++)e.setRenderTarget(this,i),e.clear(t,n,r);e.setRenderTarget(i)}},Do=new Z,Oo=new Z,ko=new X,Ao=class{constructor(e=new Z(1,0,0),t=0){this.isPlane=!0,this.normal=e,this.constant=t}set(e,t){return this.normal.copy(e),this.constant=t,this}setComponents(e,t,n,r){return this.normal.set(e,t,n),this.constant=r,this}setFromNormalAndCoplanarPoint(e,t){return this.normal.copy(e),this.constant=-t.dot(this.normal),this}setFromCoplanarPoints(e,t,n){let r=Do.subVectors(n,t).cross(Oo.subVectors(e,t)).normalize();return this.setFromNormalAndCoplanarPoint(r,e),this}copy(e){return this.normal.copy(e.normal),this.constant=e.constant,this}normalize(){let e=1/this.normal.length();return this.normal.multiplyScalar(e),this.constant*=e,this}negate(){return this.constant*=-1,this.normal.negate(),this}distanceToPoint(e){return this.normal.dot(e)+this.constant}distanceToSphere(e){return this.distanceToPoint(e.center)-e.radius}projectPoint(e,t){return t.copy(e).addScaledVector(this.normal,-this.distanceToPoint(e))}intersectLine(e,t){let n=e.delta(Do),r=this.normal.dot(n);if(r===0)return this.distanceToPoint(e.start)===0?t.copy(e.start):null;let i=-(e.start.dot(this.normal)+this.constant)/r;return i<0||i>1?null:t.copy(e.start).addScaledVector(n,i)}intersectsLine(e){let t=this.distanceToPoint(e.start),n=this.distanceToPoint(e.end);return t<0&&n>0||n<0&&t>0}intersectsBox(e){return e.intersectsPlane(this)}intersectsSphere(e){return e.intersectsPlane(this)}coplanarPoint(e){return e.copy(this.normal).multiplyScalar(-this.constant)}applyMatrix4(e,t){let n=t||ko.getNormalMatrix(e),r=this.coplanarPoint(Do).applyMatrix4(e),i=this.normal.applyMatrix3(n).normalize();return this.constant=-r.dot(i),this}translate(e){return this.constant-=e.dot(this.normal),this}equals(e){return e.normal.equals(this.normal)&&e.constant===this.constant}clone(){return new this.constructor().copy(this)}},jo=new wi,Mo=new Z,No=class{constructor(e=new Ao,t=new Ao,n=new Ao,r=new Ao,i=new Ao,a=new Ao){this.planes=[e,t,n,r,i,a]}set(e,t,n,r,i,a){let o=this.planes;return o[0].copy(e),o[1].copy(t),o[2].copy(n),o[3].copy(r),o[4].copy(i),o[5].copy(a),this}copy(e){let t=this.planes;for(let n=0;n<6;n++)t[n].copy(e.planes[n]);return this}setFromProjectionMatrix(e,t=rr){let n=this.planes,r=e.elements,i=r[0],a=r[1],o=r[2],s=r[3],c=r[4],l=r[5],u=r[6],d=r[7],f=r[8],p=r[9],m=r[10],h=r[11],g=r[12],_=r[13],v=r[14],y=r[15];if(n[0].setComponents(s-i,d-c,h-f,y-g).normalize(),n[1].setComponents(s+i,d+c,h+f,y+g).normalize(),n[2].setComponents(s+a,d+l,h+p,y+_).normalize(),n[3].setComponents(s-a,d-l,h-p,y-_).normalize(),n[4].setComponents(s-o,d-u,h-m,y-v).normalize(),t===2e3)n[5].setComponents(s+o,d+u,h+m,y+v).normalize();else if(t===2001)n[5].setComponents(o,u,m,v).normalize();else throw Error(`THREE.Frustum.setFromProjectionMatrix(): Invalid coordinate system: `+t);return this}intersectsObject(e){if(e.boundingSphere!==void 0)e.boundingSphere===null&&e.computeBoundingSphere(),jo.copy(e.boundingSphere).applyMatrix4(e.matrixWorld);else{let t=e.geometry;t.boundingSphere===null&&t.computeBoundingSphere(),jo.copy(t.boundingSphere).applyMatrix4(e.matrixWorld)}return this.intersectsSphere(jo)}intersectsSprite(e){return jo.center.set(0,0,0),jo.radius=.7071067811865476,jo.applyMatrix4(e.matrixWorld),this.intersectsSphere(jo)}intersectsSphere(e){let t=this.planes,n=e.center,r=-e.radius;for(let e=0;e<6;e++)if(t[e].distanceToPoint(n)<r)return!1;return!0}intersectsBox(e){let t=this.planes;for(let n=0;n<6;n++){let r=t[n];if(Mo.x=r.normal.x>0?e.max.x:e.min.x,Mo.y=r.normal.y>0?e.max.y:e.min.y,Mo.z=r.normal.z>0?e.max.z:e.min.z,r.distanceToPoint(Mo)<0)return!1}return!0}containsPoint(e){let t=this.planes;for(let n=0;n<6;n++)if(t[n].distanceToPoint(e)<0)return!1;return!0}clone(){return new this.constructor().copy(this)}};function Po(){let e=null,t=!1,n=null,r=null;function i(t,a){n(t,a),r=e.requestAnimationFrame(i)}return{start:function(){t!==!0&&n!==null&&(r=e.requestAnimationFrame(i),t=!0)},stop:function(){e.cancelAnimationFrame(r),t=!1},setAnimationLoop:function(e){n=e},setContext:function(t){e=t}}}function Fo(e,t){let n=t.isWebGL2,r=new WeakMap;function i(t,r){let i=t.array,a=t.usage,o=i.byteLength,s=e.createBuffer();e.bindBuffer(r,s),e.bufferData(r,i,a),t.onUploadCallback();let c;if(i instanceof Float32Array)c=e.FLOAT;else if(i instanceof Uint16Array)if(t.isFloat16BufferAttribute)if(n)c=e.HALF_FLOAT;else throw Error(`THREE.WebGLAttributes: Usage of Float16BufferAttribute requires WebGL2.`);else c=e.UNSIGNED_SHORT;else if(i instanceof Int16Array)c=e.SHORT;else if(i instanceof Uint32Array)c=e.UNSIGNED_INT;else if(i instanceof Int32Array)c=e.INT;else if(i instanceof Int8Array)c=e.BYTE;else if(i instanceof Uint8Array)c=e.UNSIGNED_BYTE;else if(i instanceof Uint8ClampedArray)c=e.UNSIGNED_BYTE;else throw Error(`THREE.WebGLAttributes: Unsupported buffer data format: `+i);return{buffer:s,type:c,bytesPerElement:i.BYTES_PER_ELEMENT,version:t.version,size:o}}function a(t,r,i){let a=r.array,o=r._updateRange,s=r.updateRanges;if(e.bindBuffer(i,t),o.count===-1&&s.length===0&&e.bufferSubData(i,0,a),s.length!==0){for(let t=0,r=s.length;t<r;t++){let r=s[t];n?e.bufferSubData(i,r.start*a.BYTES_PER_ELEMENT,a,r.start,r.count):e.bufferSubData(i,r.start*a.BYTES_PER_ELEMENT,a.subarray(r.start,r.start+r.count))}r.clearUpdateRanges()}o.count!==-1&&(n?e.bufferSubData(i,o.offset*a.BYTES_PER_ELEMENT,a,o.offset,o.count):e.bufferSubData(i,o.offset*a.BYTES_PER_ELEMENT,a.subarray(o.offset,o.offset+o.count)),o.count=-1),r.onUploadCallback()}function o(e){return e.isInterleavedBufferAttribute&&(e=e.data),r.get(e)}function s(t){t.isInterleavedBufferAttribute&&(t=t.data);let n=r.get(t);n&&(e.deleteBuffer(n.buffer),r.delete(t))}function c(e,t){if(e.isGLBufferAttribute){let t=r.get(e);(!t||t.version<e.version)&&r.set(e,{buffer:e.buffer,type:e.type,bytesPerElement:e.elementSize,version:e.version});return}e.isInterleavedBufferAttribute&&(e=e.data);let n=r.get(e);if(n===void 0)r.set(e,i(e,t));else if(n.version<e.version){if(n.size!==e.array.byteLength)throw Error(`THREE.WebGLAttributes: The size of the buffer attribute's array buffer does not match the original size. Resizing buffer attributes is not supported.`);a(n.buffer,e,t),n.version=e.version}}return{get:o,remove:s,update:c}}var Io=class e extends Ba{constructor(e=1,t=1,n=1,r=1){super(),this.type=`PlaneGeometry`,this.parameters={width:e,height:t,widthSegments:n,heightSegments:r};let i=e/2,a=t/2,o=Math.floor(n),s=Math.floor(r),c=o+1,l=s+1,u=e/o,d=t/s,f=[],p=[],m=[],h=[];for(let e=0;e<l;e++){let t=e*d-a;for(let n=0;n<c;n++){let r=n*u-i;p.push(r,-t,0),m.push(0,0,1),h.push(n/o),h.push(1-e/s)}}for(let e=0;e<s;e++)for(let t=0;t<o;t++){let n=t+c*e,r=t+c*(e+1),i=t+1+c*(e+1),a=t+1+c*e;f.push(n,r,a),f.push(r,i,a)}this.setIndex(f),this.setAttribute(`position`,new Ma(p,3)),this.setAttribute(`normal`,new Ma(m,3)),this.setAttribute(`uv`,new Ma(h,2))}copy(e){return super.copy(e),this.parameters=Object.assign({},e.parameters),this}static fromJSON(t){return new e(t.width,t.height,t.widthSegments,t.heightSegments)}},Q={alphahash_fragment:`#ifdef USE_ALPHAHASH
	if ( diffuseColor.a < getAlphaHashThreshold( vPosition ) ) discard;
#endif`,alphahash_pars_fragment:`#ifdef USE_ALPHAHASH
	const float ALPHA_HASH_SCALE = 0.05;
	float hash2D( vec2 value ) {
		return fract( 1.0e4 * sin( 17.0 * value.x + 0.1 * value.y ) * ( 0.1 + abs( sin( 13.0 * value.y + value.x ) ) ) );
	}
	float hash3D( vec3 value ) {
		return hash2D( vec2( hash2D( value.xy ), value.z ) );
	}
	float getAlphaHashThreshold( vec3 position ) {
		float maxDeriv = max(
			length( dFdx( position.xyz ) ),
			length( dFdy( position.xyz ) )
		);
		float pixScale = 1.0 / ( ALPHA_HASH_SCALE * maxDeriv );
		vec2 pixScales = vec2(
			exp2( floor( log2( pixScale ) ) ),
			exp2( ceil( log2( pixScale ) ) )
		);
		vec2 alpha = vec2(
			hash3D( floor( pixScales.x * position.xyz ) ),
			hash3D( floor( pixScales.y * position.xyz ) )
		);
		float lerpFactor = fract( log2( pixScale ) );
		float x = ( 1.0 - lerpFactor ) * alpha.x + lerpFactor * alpha.y;
		float a = min( lerpFactor, 1.0 - lerpFactor );
		vec3 cases = vec3(
			x * x / ( 2.0 * a * ( 1.0 - a ) ),
			( x - 0.5 * a ) / ( 1.0 - a ),
			1.0 - ( ( 1.0 - x ) * ( 1.0 - x ) / ( 2.0 * a * ( 1.0 - a ) ) )
		);
		float threshold = ( x < ( 1.0 - a ) )
			? ( ( x < a ) ? cases.x : cases.y )
			: cases.z;
		return clamp( threshold , 1.0e-6, 1.0 );
	}
#endif`,alphamap_fragment:`#ifdef USE_ALPHAMAP
	diffuseColor.a *= texture2D( alphaMap, vAlphaMapUv ).g;
#endif`,alphamap_pars_fragment:`#ifdef USE_ALPHAMAP
	uniform sampler2D alphaMap;
#endif`,alphatest_fragment:`#ifdef USE_ALPHATEST
	#ifdef ALPHA_TO_COVERAGE
	diffuseColor.a = smoothstep( alphaTest, alphaTest + fwidth( diffuseColor.a ), diffuseColor.a );
	if ( diffuseColor.a == 0.0 ) discard;
	#else
	if ( diffuseColor.a < alphaTest ) discard;
	#endif
#endif`,alphatest_pars_fragment:`#ifdef USE_ALPHATEST
	uniform float alphaTest;
#endif`,aomap_fragment:`#ifdef USE_AOMAP
	float ambientOcclusion = ( texture2D( aoMap, vAoMapUv ).r - 1.0 ) * aoMapIntensity + 1.0;
	reflectedLight.indirectDiffuse *= ambientOcclusion;
	#if defined( USE_CLEARCOAT ) 
		clearcoatSpecularIndirect *= ambientOcclusion;
	#endif
	#if defined( USE_SHEEN ) 
		sheenSpecularIndirect *= ambientOcclusion;
	#endif
	#if defined( USE_ENVMAP ) && defined( STANDARD )
		float dotNV = saturate( dot( geometryNormal, geometryViewDir ) );
		reflectedLight.indirectSpecular *= computeSpecularOcclusion( dotNV, ambientOcclusion, material.roughness );
	#endif
#endif`,aomap_pars_fragment:`#ifdef USE_AOMAP
	uniform sampler2D aoMap;
	uniform float aoMapIntensity;
#endif`,batching_pars_vertex:`#ifdef USE_BATCHING
	attribute float batchId;
	uniform highp sampler2D batchingTexture;
	mat4 getBatchingMatrix( const in float i ) {
		int size = textureSize( batchingTexture, 0 ).x;
		int j = int( i ) * 4;
		int x = j % size;
		int y = j / size;
		vec4 v1 = texelFetch( batchingTexture, ivec2( x, y ), 0 );
		vec4 v2 = texelFetch( batchingTexture, ivec2( x + 1, y ), 0 );
		vec4 v3 = texelFetch( batchingTexture, ivec2( x + 2, y ), 0 );
		vec4 v4 = texelFetch( batchingTexture, ivec2( x + 3, y ), 0 );
		return mat4( v1, v2, v3, v4 );
	}
#endif`,batching_vertex:`#ifdef USE_BATCHING
	mat4 batchingMatrix = getBatchingMatrix( batchId );
#endif`,begin_vertex:`vec3 transformed = vec3( position );
#ifdef USE_ALPHAHASH
	vPosition = vec3( position );
#endif`,beginnormal_vertex:`vec3 objectNormal = vec3( normal );
#ifdef USE_TANGENT
	vec3 objectTangent = vec3( tangent.xyz );
#endif`,bsdfs:`float G_BlinnPhong_Implicit( ) {
	return 0.25;
}
float D_BlinnPhong( const in float shininess, const in float dotNH ) {
	return RECIPROCAL_PI * ( shininess * 0.5 + 1.0 ) * pow( dotNH, shininess );
}
vec3 BRDF_BlinnPhong( const in vec3 lightDir, const in vec3 viewDir, const in vec3 normal, const in vec3 specularColor, const in float shininess ) {
	vec3 halfDir = normalize( lightDir + viewDir );
	float dotNH = saturate( dot( normal, halfDir ) );
	float dotVH = saturate( dot( viewDir, halfDir ) );
	vec3 F = F_Schlick( specularColor, 1.0, dotVH );
	float G = G_BlinnPhong_Implicit( );
	float D = D_BlinnPhong( shininess, dotNH );
	return F * ( G * D );
} // validated`,iridescence_fragment:`#ifdef USE_IRIDESCENCE
	const mat3 XYZ_TO_REC709 = mat3(
		 3.2404542, -0.9692660,  0.0556434,
		-1.5371385,  1.8760108, -0.2040259,
		-0.4985314,  0.0415560,  1.0572252
	);
	vec3 Fresnel0ToIor( vec3 fresnel0 ) {
		vec3 sqrtF0 = sqrt( fresnel0 );
		return ( vec3( 1.0 ) + sqrtF0 ) / ( vec3( 1.0 ) - sqrtF0 );
	}
	vec3 IorToFresnel0( vec3 transmittedIor, float incidentIor ) {
		return pow2( ( transmittedIor - vec3( incidentIor ) ) / ( transmittedIor + vec3( incidentIor ) ) );
	}
	float IorToFresnel0( float transmittedIor, float incidentIor ) {
		return pow2( ( transmittedIor - incidentIor ) / ( transmittedIor + incidentIor ));
	}
	vec3 evalSensitivity( float OPD, vec3 shift ) {
		float phase = 2.0 * PI * OPD * 1.0e-9;
		vec3 val = vec3( 5.4856e-13, 4.4201e-13, 5.2481e-13 );
		vec3 pos = vec3( 1.6810e+06, 1.7953e+06, 2.2084e+06 );
		vec3 var = vec3( 4.3278e+09, 9.3046e+09, 6.6121e+09 );
		vec3 xyz = val * sqrt( 2.0 * PI * var ) * cos( pos * phase + shift ) * exp( - pow2( phase ) * var );
		xyz.x += 9.7470e-14 * sqrt( 2.0 * PI * 4.5282e+09 ) * cos( 2.2399e+06 * phase + shift[ 0 ] ) * exp( - 4.5282e+09 * pow2( phase ) );
		xyz /= 1.0685e-7;
		vec3 rgb = XYZ_TO_REC709 * xyz;
		return rgb;
	}
	vec3 evalIridescence( float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0 ) {
		vec3 I;
		float iridescenceIOR = mix( outsideIOR, eta2, smoothstep( 0.0, 0.03, thinFilmThickness ) );
		float sinTheta2Sq = pow2( outsideIOR / iridescenceIOR ) * ( 1.0 - pow2( cosTheta1 ) );
		float cosTheta2Sq = 1.0 - sinTheta2Sq;
		if ( cosTheta2Sq < 0.0 ) {
			return vec3( 1.0 );
		}
		float cosTheta2 = sqrt( cosTheta2Sq );
		float R0 = IorToFresnel0( iridescenceIOR, outsideIOR );
		float R12 = F_Schlick( R0, 1.0, cosTheta1 );
		float T121 = 1.0 - R12;
		float phi12 = 0.0;
		if ( iridescenceIOR < outsideIOR ) phi12 = PI;
		float phi21 = PI - phi12;
		vec3 baseIOR = Fresnel0ToIor( clamp( baseF0, 0.0, 0.9999 ) );		vec3 R1 = IorToFresnel0( baseIOR, iridescenceIOR );
		vec3 R23 = F_Schlick( R1, 1.0, cosTheta2 );
		vec3 phi23 = vec3( 0.0 );
		if ( baseIOR[ 0 ] < iridescenceIOR ) phi23[ 0 ] = PI;
		if ( baseIOR[ 1 ] < iridescenceIOR ) phi23[ 1 ] = PI;
		if ( baseIOR[ 2 ] < iridescenceIOR ) phi23[ 2 ] = PI;
		float OPD = 2.0 * iridescenceIOR * thinFilmThickness * cosTheta2;
		vec3 phi = vec3( phi21 ) + phi23;
		vec3 R123 = clamp( R12 * R23, 1e-5, 0.9999 );
		vec3 r123 = sqrt( R123 );
		vec3 Rs = pow2( T121 ) * R23 / ( vec3( 1.0 ) - R123 );
		vec3 C0 = R12 + Rs;
		I = C0;
		vec3 Cm = Rs - T121;
		for ( int m = 1; m <= 2; ++ m ) {
			Cm *= r123;
			vec3 Sm = 2.0 * evalSensitivity( float( m ) * OPD, float( m ) * phi );
			I += Cm * Sm;
		}
		return max( I, vec3( 0.0 ) );
	}
#endif`,bumpmap_pars_fragment:`#ifdef USE_BUMPMAP
	uniform sampler2D bumpMap;
	uniform float bumpScale;
	vec2 dHdxy_fwd() {
		vec2 dSTdx = dFdx( vBumpMapUv );
		vec2 dSTdy = dFdy( vBumpMapUv );
		float Hll = bumpScale * texture2D( bumpMap, vBumpMapUv ).x;
		float dBx = bumpScale * texture2D( bumpMap, vBumpMapUv + dSTdx ).x - Hll;
		float dBy = bumpScale * texture2D( bumpMap, vBumpMapUv + dSTdy ).x - Hll;
		return vec2( dBx, dBy );
	}
	vec3 perturbNormalArb( vec3 surf_pos, vec3 surf_norm, vec2 dHdxy, float faceDirection ) {
		vec3 vSigmaX = normalize( dFdx( surf_pos.xyz ) );
		vec3 vSigmaY = normalize( dFdy( surf_pos.xyz ) );
		vec3 vN = surf_norm;
		vec3 R1 = cross( vSigmaY, vN );
		vec3 R2 = cross( vN, vSigmaX );
		float fDet = dot( vSigmaX, R1 ) * faceDirection;
		vec3 vGrad = sign( fDet ) * ( dHdxy.x * R1 + dHdxy.y * R2 );
		return normalize( abs( fDet ) * surf_norm - vGrad );
	}
#endif`,clipping_planes_fragment:`#if NUM_CLIPPING_PLANES > 0
	vec4 plane;
	#ifdef ALPHA_TO_COVERAGE
		float distanceToPlane, distanceGradient;
		float clipOpacity = 1.0;
		#pragma unroll_loop_start
		for ( int i = 0; i < UNION_CLIPPING_PLANES; i ++ ) {
			plane = clippingPlanes[ i ];
			distanceToPlane = - dot( vClipPosition, plane.xyz ) + plane.w;
			distanceGradient = fwidth( distanceToPlane ) / 2.0;
			clipOpacity *= smoothstep( - distanceGradient, distanceGradient, distanceToPlane );
			if ( clipOpacity == 0.0 ) discard;
		}
		#pragma unroll_loop_end
		#if UNION_CLIPPING_PLANES < NUM_CLIPPING_PLANES
			float unionClipOpacity = 1.0;
			#pragma unroll_loop_start
			for ( int i = UNION_CLIPPING_PLANES; i < NUM_CLIPPING_PLANES; i ++ ) {
				plane = clippingPlanes[ i ];
				distanceToPlane = - dot( vClipPosition, plane.xyz ) + plane.w;
				distanceGradient = fwidth( distanceToPlane ) / 2.0;
				unionClipOpacity *= 1.0 - smoothstep( - distanceGradient, distanceGradient, distanceToPlane );
			}
			#pragma unroll_loop_end
			clipOpacity *= 1.0 - unionClipOpacity;
		#endif
		diffuseColor.a *= clipOpacity;
		if ( diffuseColor.a == 0.0 ) discard;
	#else
		#pragma unroll_loop_start
		for ( int i = 0; i < UNION_CLIPPING_PLANES; i ++ ) {
			plane = clippingPlanes[ i ];
			if ( dot( vClipPosition, plane.xyz ) > plane.w ) discard;
		}
		#pragma unroll_loop_end
		#if UNION_CLIPPING_PLANES < NUM_CLIPPING_PLANES
			bool clipped = true;
			#pragma unroll_loop_start
			for ( int i = UNION_CLIPPING_PLANES; i < NUM_CLIPPING_PLANES; i ++ ) {
				plane = clippingPlanes[ i ];
				clipped = ( dot( vClipPosition, plane.xyz ) > plane.w ) && clipped;
			}
			#pragma unroll_loop_end
			if ( clipped ) discard;
		#endif
	#endif
#endif`,clipping_planes_pars_fragment:`#if NUM_CLIPPING_PLANES > 0
	varying vec3 vClipPosition;
	uniform vec4 clippingPlanes[ NUM_CLIPPING_PLANES ];
#endif`,clipping_planes_pars_vertex:`#if NUM_CLIPPING_PLANES > 0
	varying vec3 vClipPosition;
#endif`,clipping_planes_vertex:`#if NUM_CLIPPING_PLANES > 0
	vClipPosition = - mvPosition.xyz;
#endif`,color_fragment:`#if defined( USE_COLOR_ALPHA )
	diffuseColor *= vColor;
#elif defined( USE_COLOR )
	diffuseColor.rgb *= vColor;
#endif`,color_pars_fragment:`#if defined( USE_COLOR_ALPHA )
	varying vec4 vColor;
#elif defined( USE_COLOR )
	varying vec3 vColor;
#endif`,color_pars_vertex:`#if defined( USE_COLOR_ALPHA )
	varying vec4 vColor;
#elif defined( USE_COLOR ) || defined( USE_INSTANCING_COLOR )
	varying vec3 vColor;
#endif`,color_vertex:`#if defined( USE_COLOR_ALPHA )
	vColor = vec4( 1.0 );
#elif defined( USE_COLOR ) || defined( USE_INSTANCING_COLOR )
	vColor = vec3( 1.0 );
#endif
#ifdef USE_COLOR
	vColor *= color;
#endif
#ifdef USE_INSTANCING_COLOR
	vColor.xyz *= instanceColor.xyz;
#endif`,common:`#define PI 3.141592653589793
#define PI2 6.283185307179586
#define PI_HALF 1.5707963267948966
#define RECIPROCAL_PI 0.3183098861837907
#define RECIPROCAL_PI2 0.15915494309189535
#define EPSILON 1e-6
#ifndef saturate
#define saturate( a ) clamp( a, 0.0, 1.0 )
#endif
#define whiteComplement( a ) ( 1.0 - saturate( a ) )
float pow2( const in float x ) { return x*x; }
vec3 pow2( const in vec3 x ) { return x*x; }
float pow3( const in float x ) { return x*x*x; }
float pow4( const in float x ) { float x2 = x*x; return x2*x2; }
float max3( const in vec3 v ) { return max( max( v.x, v.y ), v.z ); }
float average( const in vec3 v ) { return dot( v, vec3( 0.3333333 ) ); }
highp float rand( const in vec2 uv ) {
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract( sin( sn ) * c );
}
#ifdef HIGH_PRECISION
	float precisionSafeLength( vec3 v ) { return length( v ); }
#else
	float precisionSafeLength( vec3 v ) {
		float maxComponent = max3( abs( v ) );
		return length( v / maxComponent ) * maxComponent;
	}
#endif
struct IncidentLight {
	vec3 color;
	vec3 direction;
	bool visible;
};
struct ReflectedLight {
	vec3 directDiffuse;
	vec3 directSpecular;
	vec3 indirectDiffuse;
	vec3 indirectSpecular;
};
#ifdef USE_ALPHAHASH
	varying vec3 vPosition;
#endif
vec3 transformDirection( in vec3 dir, in mat4 matrix ) {
	return normalize( ( matrix * vec4( dir, 0.0 ) ).xyz );
}
vec3 inverseTransformDirection( in vec3 dir, in mat4 matrix ) {
	return normalize( ( vec4( dir, 0.0 ) * matrix ).xyz );
}
mat3 transposeMat3( const in mat3 m ) {
	mat3 tmp;
	tmp[ 0 ] = vec3( m[ 0 ].x, m[ 1 ].x, m[ 2 ].x );
	tmp[ 1 ] = vec3( m[ 0 ].y, m[ 1 ].y, m[ 2 ].y );
	tmp[ 2 ] = vec3( m[ 0 ].z, m[ 1 ].z, m[ 2 ].z );
	return tmp;
}
float luminance( const in vec3 rgb ) {
	const vec3 weights = vec3( 0.2126729, 0.7151522, 0.0721750 );
	return dot( weights, rgb );
}
bool isPerspectiveMatrix( mat4 m ) {
	return m[ 2 ][ 3 ] == - 1.0;
}
vec2 equirectUv( in vec3 dir ) {
	float u = atan( dir.z, dir.x ) * RECIPROCAL_PI2 + 0.5;
	float v = asin( clamp( dir.y, - 1.0, 1.0 ) ) * RECIPROCAL_PI + 0.5;
	return vec2( u, v );
}
vec3 BRDF_Lambert( const in vec3 diffuseColor ) {
	return RECIPROCAL_PI * diffuseColor;
}
vec3 F_Schlick( const in vec3 f0, const in float f90, const in float dotVH ) {
	float fresnel = exp2( ( - 5.55473 * dotVH - 6.98316 ) * dotVH );
	return f0 * ( 1.0 - fresnel ) + ( f90 * fresnel );
}
float F_Schlick( const in float f0, const in float f90, const in float dotVH ) {
	float fresnel = exp2( ( - 5.55473 * dotVH - 6.98316 ) * dotVH );
	return f0 * ( 1.0 - fresnel ) + ( f90 * fresnel );
} // validated`,cube_uv_reflection_fragment:`#ifdef ENVMAP_TYPE_CUBE_UV
	#define cubeUV_minMipLevel 4.0
	#define cubeUV_minTileSize 16.0
	float getFace( vec3 direction ) {
		vec3 absDirection = abs( direction );
		float face = - 1.0;
		if ( absDirection.x > absDirection.z ) {
			if ( absDirection.x > absDirection.y )
				face = direction.x > 0.0 ? 0.0 : 3.0;
			else
				face = direction.y > 0.0 ? 1.0 : 4.0;
		} else {
			if ( absDirection.z > absDirection.y )
				face = direction.z > 0.0 ? 2.0 : 5.0;
			else
				face = direction.y > 0.0 ? 1.0 : 4.0;
		}
		return face;
	}
	vec2 getUV( vec3 direction, float face ) {
		vec2 uv;
		if ( face == 0.0 ) {
			uv = vec2( direction.z, direction.y ) / abs( direction.x );
		} else if ( face == 1.0 ) {
			uv = vec2( - direction.x, - direction.z ) / abs( direction.y );
		} else if ( face == 2.0 ) {
			uv = vec2( - direction.x, direction.y ) / abs( direction.z );
		} else if ( face == 3.0 ) {
			uv = vec2( - direction.z, direction.y ) / abs( direction.x );
		} else if ( face == 4.0 ) {
			uv = vec2( - direction.x, direction.z ) / abs( direction.y );
		} else {
			uv = vec2( direction.x, direction.y ) / abs( direction.z );
		}
		return 0.5 * ( uv + 1.0 );
	}
	vec3 bilinearCubeUV( sampler2D envMap, vec3 direction, float mipInt ) {
		float face = getFace( direction );
		float filterInt = max( cubeUV_minMipLevel - mipInt, 0.0 );
		mipInt = max( mipInt, cubeUV_minMipLevel );
		float faceSize = exp2( mipInt );
		highp vec2 uv = getUV( direction, face ) * ( faceSize - 2.0 ) + 1.0;
		if ( face > 2.0 ) {
			uv.y += faceSize;
			face -= 3.0;
		}
		uv.x += face * faceSize;
		uv.x += filterInt * 3.0 * cubeUV_minTileSize;
		uv.y += 4.0 * ( exp2( CUBEUV_MAX_MIP ) - faceSize );
		uv.x *= CUBEUV_TEXEL_WIDTH;
		uv.y *= CUBEUV_TEXEL_HEIGHT;
		#ifdef texture2DGradEXT
			return texture2DGradEXT( envMap, uv, vec2( 0.0 ), vec2( 0.0 ) ).rgb;
		#else
			return texture2D( envMap, uv ).rgb;
		#endif
	}
	#define cubeUV_r0 1.0
	#define cubeUV_m0 - 2.0
	#define cubeUV_r1 0.8
	#define cubeUV_m1 - 1.0
	#define cubeUV_r4 0.4
	#define cubeUV_m4 2.0
	#define cubeUV_r5 0.305
	#define cubeUV_m5 3.0
	#define cubeUV_r6 0.21
	#define cubeUV_m6 4.0
	float roughnessToMip( float roughness ) {
		float mip = 0.0;
		if ( roughness >= cubeUV_r1 ) {
			mip = ( cubeUV_r0 - roughness ) * ( cubeUV_m1 - cubeUV_m0 ) / ( cubeUV_r0 - cubeUV_r1 ) + cubeUV_m0;
		} else if ( roughness >= cubeUV_r4 ) {
			mip = ( cubeUV_r1 - roughness ) * ( cubeUV_m4 - cubeUV_m1 ) / ( cubeUV_r1 - cubeUV_r4 ) + cubeUV_m1;
		} else if ( roughness >= cubeUV_r5 ) {
			mip = ( cubeUV_r4 - roughness ) * ( cubeUV_m5 - cubeUV_m4 ) / ( cubeUV_r4 - cubeUV_r5 ) + cubeUV_m4;
		} else if ( roughness >= cubeUV_r6 ) {
			mip = ( cubeUV_r5 - roughness ) * ( cubeUV_m6 - cubeUV_m5 ) / ( cubeUV_r5 - cubeUV_r6 ) + cubeUV_m5;
		} else {
			mip = - 2.0 * log2( 1.16 * roughness );		}
		return mip;
	}
	vec4 textureCubeUV( sampler2D envMap, vec3 sampleDir, float roughness ) {
		float mip = clamp( roughnessToMip( roughness ), cubeUV_m0, CUBEUV_MAX_MIP );
		float mipF = fract( mip );
		float mipInt = floor( mip );
		vec3 color0 = bilinearCubeUV( envMap, sampleDir, mipInt );
		if ( mipF == 0.0 ) {
			return vec4( color0, 1.0 );
		} else {
			vec3 color1 = bilinearCubeUV( envMap, sampleDir, mipInt + 1.0 );
			return vec4( mix( color0, color1, mipF ), 1.0 );
		}
	}
#endif`,defaultnormal_vertex:`vec3 transformedNormal = objectNormal;
#ifdef USE_TANGENT
	vec3 transformedTangent = objectTangent;
#endif
#ifdef USE_BATCHING
	mat3 bm = mat3( batchingMatrix );
	transformedNormal /= vec3( dot( bm[ 0 ], bm[ 0 ] ), dot( bm[ 1 ], bm[ 1 ] ), dot( bm[ 2 ], bm[ 2 ] ) );
	transformedNormal = bm * transformedNormal;
	#ifdef USE_TANGENT
		transformedTangent = bm * transformedTangent;
	#endif
#endif
#ifdef USE_INSTANCING
	mat3 im = mat3( instanceMatrix );
	transformedNormal /= vec3( dot( im[ 0 ], im[ 0 ] ), dot( im[ 1 ], im[ 1 ] ), dot( im[ 2 ], im[ 2 ] ) );
	transformedNormal = im * transformedNormal;
	#ifdef USE_TANGENT
		transformedTangent = im * transformedTangent;
	#endif
#endif
transformedNormal = normalMatrix * transformedNormal;
#ifdef FLIP_SIDED
	transformedNormal = - transformedNormal;
#endif
#ifdef USE_TANGENT
	transformedTangent = ( modelViewMatrix * vec4( transformedTangent, 0.0 ) ).xyz;
	#ifdef FLIP_SIDED
		transformedTangent = - transformedTangent;
	#endif
#endif`,displacementmap_pars_vertex:`#ifdef USE_DISPLACEMENTMAP
	uniform sampler2D displacementMap;
	uniform float displacementScale;
	uniform float displacementBias;
#endif`,displacementmap_vertex:`#ifdef USE_DISPLACEMENTMAP
	transformed += normalize( objectNormal ) * ( texture2D( displacementMap, vDisplacementMapUv ).x * displacementScale + displacementBias );
#endif`,emissivemap_fragment:`#ifdef USE_EMISSIVEMAP
	vec4 emissiveColor = texture2D( emissiveMap, vEmissiveMapUv );
	totalEmissiveRadiance *= emissiveColor.rgb;
#endif`,emissivemap_pars_fragment:`#ifdef USE_EMISSIVEMAP
	uniform sampler2D emissiveMap;
#endif`,colorspace_fragment:`gl_FragColor = linearToOutputTexel( gl_FragColor );`,colorspace_pars_fragment:`
const mat3 LINEAR_SRGB_TO_LINEAR_DISPLAY_P3 = mat3(
	vec3( 0.8224621, 0.177538, 0.0 ),
	vec3( 0.0331941, 0.9668058, 0.0 ),
	vec3( 0.0170827, 0.0723974, 0.9105199 )
);
const mat3 LINEAR_DISPLAY_P3_TO_LINEAR_SRGB = mat3(
	vec3( 1.2249401, - 0.2249404, 0.0 ),
	vec3( - 0.0420569, 1.0420571, 0.0 ),
	vec3( - 0.0196376, - 0.0786361, 1.0982735 )
);
vec4 LinearSRGBToLinearDisplayP3( in vec4 value ) {
	return vec4( value.rgb * LINEAR_SRGB_TO_LINEAR_DISPLAY_P3, value.a );
}
vec4 LinearDisplayP3ToLinearSRGB( in vec4 value ) {
	return vec4( value.rgb * LINEAR_DISPLAY_P3_TO_LINEAR_SRGB, value.a );
}
vec4 LinearTransferOETF( in vec4 value ) {
	return value;
}
vec4 sRGBTransferOETF( in vec4 value ) {
	return vec4( mix( pow( value.rgb, vec3( 0.41666 ) ) * 1.055 - vec3( 0.055 ), value.rgb * 12.92, vec3( lessThanEqual( value.rgb, vec3( 0.0031308 ) ) ) ), value.a );
}
vec4 LinearToLinear( in vec4 value ) {
	return value;
}
vec4 LinearTosRGB( in vec4 value ) {
	return sRGBTransferOETF( value );
}`,envmap_fragment:`#ifdef USE_ENVMAP
	#ifdef ENV_WORLDPOS
		vec3 cameraToFrag;
		if ( isOrthographic ) {
			cameraToFrag = normalize( vec3( - viewMatrix[ 0 ][ 2 ], - viewMatrix[ 1 ][ 2 ], - viewMatrix[ 2 ][ 2 ] ) );
		} else {
			cameraToFrag = normalize( vWorldPosition - cameraPosition );
		}
		vec3 worldNormal = inverseTransformDirection( normal, viewMatrix );
		#ifdef ENVMAP_MODE_REFLECTION
			vec3 reflectVec = reflect( cameraToFrag, worldNormal );
		#else
			vec3 reflectVec = refract( cameraToFrag, worldNormal, refractionRatio );
		#endif
	#else
		vec3 reflectVec = vReflect;
	#endif
	#ifdef ENVMAP_TYPE_CUBE
		vec4 envColor = textureCube( envMap, envMapRotation * vec3( flipEnvMap * reflectVec.x, reflectVec.yz ) );
	#else
		vec4 envColor = vec4( 0.0 );
	#endif
	#ifdef ENVMAP_BLENDING_MULTIPLY
		outgoingLight = mix( outgoingLight, outgoingLight * envColor.xyz, specularStrength * reflectivity );
	#elif defined( ENVMAP_BLENDING_MIX )
		outgoingLight = mix( outgoingLight, envColor.xyz, specularStrength * reflectivity );
	#elif defined( ENVMAP_BLENDING_ADD )
		outgoingLight += envColor.xyz * specularStrength * reflectivity;
	#endif
#endif`,envmap_common_pars_fragment:`#ifdef USE_ENVMAP
	uniform float envMapIntensity;
	uniform float flipEnvMap;
	uniform mat3 envMapRotation;
	#ifdef ENVMAP_TYPE_CUBE
		uniform samplerCube envMap;
	#else
		uniform sampler2D envMap;
	#endif
	
#endif`,envmap_pars_fragment:`#ifdef USE_ENVMAP
	uniform float reflectivity;
	#if defined( USE_BUMPMAP ) || defined( USE_NORMALMAP ) || defined( PHONG ) || defined( LAMBERT )
		#define ENV_WORLDPOS
	#endif
	#ifdef ENV_WORLDPOS
		varying vec3 vWorldPosition;
		uniform float refractionRatio;
	#else
		varying vec3 vReflect;
	#endif
#endif`,envmap_pars_vertex:`#ifdef USE_ENVMAP
	#if defined( USE_BUMPMAP ) || defined( USE_NORMALMAP ) || defined( PHONG ) || defined( LAMBERT )
		#define ENV_WORLDPOS
	#endif
	#ifdef ENV_WORLDPOS
		
		varying vec3 vWorldPosition;
	#else
		varying vec3 vReflect;
		uniform float refractionRatio;
	#endif
#endif`,envmap_physical_pars_fragment:`#ifdef USE_ENVMAP
	vec3 getIBLIrradiance( const in vec3 normal ) {
		#ifdef ENVMAP_TYPE_CUBE_UV
			vec3 worldNormal = inverseTransformDirection( normal, viewMatrix );
			vec4 envMapColor = textureCubeUV( envMap, envMapRotation * worldNormal, 1.0 );
			return PI * envMapColor.rgb * envMapIntensity;
		#else
			return vec3( 0.0 );
		#endif
	}
	vec3 getIBLRadiance( const in vec3 viewDir, const in vec3 normal, const in float roughness ) {
		#ifdef ENVMAP_TYPE_CUBE_UV
			vec3 reflectVec = reflect( - viewDir, normal );
			reflectVec = normalize( mix( reflectVec, normal, roughness * roughness) );
			reflectVec = inverseTransformDirection( reflectVec, viewMatrix );
			vec4 envMapColor = textureCubeUV( envMap, envMapRotation * reflectVec, roughness );
			return envMapColor.rgb * envMapIntensity;
		#else
			return vec3( 0.0 );
		#endif
	}
	#ifdef USE_ANISOTROPY
		vec3 getIBLAnisotropyRadiance( const in vec3 viewDir, const in vec3 normal, const in float roughness, const in vec3 bitangent, const in float anisotropy ) {
			#ifdef ENVMAP_TYPE_CUBE_UV
				vec3 bentNormal = cross( bitangent, viewDir );
				bentNormal = normalize( cross( bentNormal, bitangent ) );
				bentNormal = normalize( mix( bentNormal, normal, pow2( pow2( 1.0 - anisotropy * ( 1.0 - roughness ) ) ) ) );
				return getIBLRadiance( viewDir, bentNormal, roughness );
			#else
				return vec3( 0.0 );
			#endif
		}
	#endif
#endif`,envmap_vertex:`#ifdef USE_ENVMAP
	#ifdef ENV_WORLDPOS
		vWorldPosition = worldPosition.xyz;
	#else
		vec3 cameraToVertex;
		if ( isOrthographic ) {
			cameraToVertex = normalize( vec3( - viewMatrix[ 0 ][ 2 ], - viewMatrix[ 1 ][ 2 ], - viewMatrix[ 2 ][ 2 ] ) );
		} else {
			cameraToVertex = normalize( worldPosition.xyz - cameraPosition );
		}
		vec3 worldNormal = inverseTransformDirection( transformedNormal, viewMatrix );
		#ifdef ENVMAP_MODE_REFLECTION
			vReflect = reflect( cameraToVertex, worldNormal );
		#else
			vReflect = refract( cameraToVertex, worldNormal, refractionRatio );
		#endif
	#endif
#endif`,fog_vertex:`#ifdef USE_FOG
	vFogDepth = - mvPosition.z;
#endif`,fog_pars_vertex:`#ifdef USE_FOG
	varying float vFogDepth;
#endif`,fog_fragment:`#ifdef USE_FOG
	#ifdef FOG_EXP2
		float fogFactor = 1.0 - exp( - fogDensity * fogDensity * vFogDepth * vFogDepth );
	#else
		float fogFactor = smoothstep( fogNear, fogFar, vFogDepth );
	#endif
	gl_FragColor.rgb = mix( gl_FragColor.rgb, fogColor, fogFactor );
#endif`,fog_pars_fragment:`#ifdef USE_FOG
	uniform vec3 fogColor;
	varying float vFogDepth;
	#ifdef FOG_EXP2
		uniform float fogDensity;
	#else
		uniform float fogNear;
		uniform float fogFar;
	#endif
#endif`,gradientmap_pars_fragment:`#ifdef USE_GRADIENTMAP
	uniform sampler2D gradientMap;
#endif
vec3 getGradientIrradiance( vec3 normal, vec3 lightDirection ) {
	float dotNL = dot( normal, lightDirection );
	vec2 coord = vec2( dotNL * 0.5 + 0.5, 0.0 );
	#ifdef USE_GRADIENTMAP
		return vec3( texture2D( gradientMap, coord ).r );
	#else
		vec2 fw = fwidth( coord ) * 0.5;
		return mix( vec3( 0.7 ), vec3( 1.0 ), smoothstep( 0.7 - fw.x, 0.7 + fw.x, coord.x ) );
	#endif
}`,lightmap_fragment:`#ifdef USE_LIGHTMAP
	vec4 lightMapTexel = texture2D( lightMap, vLightMapUv );
	vec3 lightMapIrradiance = lightMapTexel.rgb * lightMapIntensity;
	reflectedLight.indirectDiffuse += lightMapIrradiance;
#endif`,lightmap_pars_fragment:`#ifdef USE_LIGHTMAP
	uniform sampler2D lightMap;
	uniform float lightMapIntensity;
#endif`,lights_lambert_fragment:`LambertMaterial material;
material.diffuseColor = diffuseColor.rgb;
material.specularStrength = specularStrength;`,lights_lambert_pars_fragment:`varying vec3 vViewPosition;
struct LambertMaterial {
	vec3 diffuseColor;
	float specularStrength;
};
void RE_Direct_Lambert( const in IncidentLight directLight, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in LambertMaterial material, inout ReflectedLight reflectedLight ) {
	float dotNL = saturate( dot( geometryNormal, directLight.direction ) );
	vec3 irradiance = dotNL * directLight.color;
	reflectedLight.directDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
void RE_IndirectDiffuse_Lambert( const in vec3 irradiance, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in LambertMaterial material, inout ReflectedLight reflectedLight ) {
	reflectedLight.indirectDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
#define RE_Direct				RE_Direct_Lambert
#define RE_IndirectDiffuse		RE_IndirectDiffuse_Lambert`,lights_pars_begin:`uniform bool receiveShadow;
uniform vec3 ambientLightColor;
#if defined( USE_LIGHT_PROBES )
	uniform vec3 lightProbe[ 9 ];
#endif
vec3 shGetIrradianceAt( in vec3 normal, in vec3 shCoefficients[ 9 ] ) {
	float x = normal.x, y = normal.y, z = normal.z;
	vec3 result = shCoefficients[ 0 ] * 0.886227;
	result += shCoefficients[ 1 ] * 2.0 * 0.511664 * y;
	result += shCoefficients[ 2 ] * 2.0 * 0.511664 * z;
	result += shCoefficients[ 3 ] * 2.0 * 0.511664 * x;
	result += shCoefficients[ 4 ] * 2.0 * 0.429043 * x * y;
	result += shCoefficients[ 5 ] * 2.0 * 0.429043 * y * z;
	result += shCoefficients[ 6 ] * ( 0.743125 * z * z - 0.247708 );
	result += shCoefficients[ 7 ] * 2.0 * 0.429043 * x * z;
	result += shCoefficients[ 8 ] * 0.429043 * ( x * x - y * y );
	return result;
}
vec3 getLightProbeIrradiance( const in vec3 lightProbe[ 9 ], const in vec3 normal ) {
	vec3 worldNormal = inverseTransformDirection( normal, viewMatrix );
	vec3 irradiance = shGetIrradianceAt( worldNormal, lightProbe );
	return irradiance;
}
vec3 getAmbientLightIrradiance( const in vec3 ambientLightColor ) {
	vec3 irradiance = ambientLightColor;
	return irradiance;
}
float getDistanceAttenuation( const in float lightDistance, const in float cutoffDistance, const in float decayExponent ) {
	#if defined ( LEGACY_LIGHTS )
		if ( cutoffDistance > 0.0 && decayExponent > 0.0 ) {
			return pow( saturate( - lightDistance / cutoffDistance + 1.0 ), decayExponent );
		}
		return 1.0;
	#else
		float distanceFalloff = 1.0 / max( pow( lightDistance, decayExponent ), 0.01 );
		if ( cutoffDistance > 0.0 ) {
			distanceFalloff *= pow2( saturate( 1.0 - pow4( lightDistance / cutoffDistance ) ) );
		}
		return distanceFalloff;
	#endif
}
float getSpotAttenuation( const in float coneCosine, const in float penumbraCosine, const in float angleCosine ) {
	return smoothstep( coneCosine, penumbraCosine, angleCosine );
}
#if NUM_DIR_LIGHTS > 0
	struct DirectionalLight {
		vec3 direction;
		vec3 color;
	};
	uniform DirectionalLight directionalLights[ NUM_DIR_LIGHTS ];
	void getDirectionalLightInfo( const in DirectionalLight directionalLight, out IncidentLight light ) {
		light.color = directionalLight.color;
		light.direction = directionalLight.direction;
		light.visible = true;
	}
#endif
#if NUM_POINT_LIGHTS > 0
	struct PointLight {
		vec3 position;
		vec3 color;
		float distance;
		float decay;
	};
	uniform PointLight pointLights[ NUM_POINT_LIGHTS ];
	void getPointLightInfo( const in PointLight pointLight, const in vec3 geometryPosition, out IncidentLight light ) {
		vec3 lVector = pointLight.position - geometryPosition;
		light.direction = normalize( lVector );
		float lightDistance = length( lVector );
		light.color = pointLight.color;
		light.color *= getDistanceAttenuation( lightDistance, pointLight.distance, pointLight.decay );
		light.visible = ( light.color != vec3( 0.0 ) );
	}
#endif
#if NUM_SPOT_LIGHTS > 0
	struct SpotLight {
		vec3 position;
		vec3 direction;
		vec3 color;
		float distance;
		float decay;
		float coneCos;
		float penumbraCos;
	};
	uniform SpotLight spotLights[ NUM_SPOT_LIGHTS ];
	void getSpotLightInfo( const in SpotLight spotLight, const in vec3 geometryPosition, out IncidentLight light ) {
		vec3 lVector = spotLight.position - geometryPosition;
		light.direction = normalize( lVector );
		float angleCos = dot( light.direction, spotLight.direction );
		float spotAttenuation = getSpotAttenuation( spotLight.coneCos, spotLight.penumbraCos, angleCos );
		if ( spotAttenuation > 0.0 ) {
			float lightDistance = length( lVector );
			light.color = spotLight.color * spotAttenuation;
			light.color *= getDistanceAttenuation( lightDistance, spotLight.distance, spotLight.decay );
			light.visible = ( light.color != vec3( 0.0 ) );
		} else {
			light.color = vec3( 0.0 );
			light.visible = false;
		}
	}
#endif
#if NUM_RECT_AREA_LIGHTS > 0
	struct RectAreaLight {
		vec3 color;
		vec3 position;
		vec3 halfWidth;
		vec3 halfHeight;
	};
	uniform sampler2D ltc_1;	uniform sampler2D ltc_2;
	uniform RectAreaLight rectAreaLights[ NUM_RECT_AREA_LIGHTS ];
#endif
#if NUM_HEMI_LIGHTS > 0
	struct HemisphereLight {
		vec3 direction;
		vec3 skyColor;
		vec3 groundColor;
	};
	uniform HemisphereLight hemisphereLights[ NUM_HEMI_LIGHTS ];
	vec3 getHemisphereLightIrradiance( const in HemisphereLight hemiLight, const in vec3 normal ) {
		float dotNL = dot( normal, hemiLight.direction );
		float hemiDiffuseWeight = 0.5 * dotNL + 0.5;
		vec3 irradiance = mix( hemiLight.groundColor, hemiLight.skyColor, hemiDiffuseWeight );
		return irradiance;
	}
#endif`,lights_toon_fragment:`ToonMaterial material;
material.diffuseColor = diffuseColor.rgb;`,lights_toon_pars_fragment:`varying vec3 vViewPosition;
struct ToonMaterial {
	vec3 diffuseColor;
};
void RE_Direct_Toon( const in IncidentLight directLight, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in ToonMaterial material, inout ReflectedLight reflectedLight ) {
	vec3 irradiance = getGradientIrradiance( geometryNormal, directLight.direction ) * directLight.color;
	reflectedLight.directDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
void RE_IndirectDiffuse_Toon( const in vec3 irradiance, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in ToonMaterial material, inout ReflectedLight reflectedLight ) {
	reflectedLight.indirectDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
#define RE_Direct				RE_Direct_Toon
#define RE_IndirectDiffuse		RE_IndirectDiffuse_Toon`,lights_phong_fragment:`BlinnPhongMaterial material;
material.diffuseColor = diffuseColor.rgb;
material.specularColor = specular;
material.specularShininess = shininess;
material.specularStrength = specularStrength;`,lights_phong_pars_fragment:`varying vec3 vViewPosition;
struct BlinnPhongMaterial {
	vec3 diffuseColor;
	vec3 specularColor;
	float specularShininess;
	float specularStrength;
};
void RE_Direct_BlinnPhong( const in IncidentLight directLight, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in BlinnPhongMaterial material, inout ReflectedLight reflectedLight ) {
	float dotNL = saturate( dot( geometryNormal, directLight.direction ) );
	vec3 irradiance = dotNL * directLight.color;
	reflectedLight.directDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
	reflectedLight.directSpecular += irradiance * BRDF_BlinnPhong( directLight.direction, geometryViewDir, geometryNormal, material.specularColor, material.specularShininess ) * material.specularStrength;
}
void RE_IndirectDiffuse_BlinnPhong( const in vec3 irradiance, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in BlinnPhongMaterial material, inout ReflectedLight reflectedLight ) {
	reflectedLight.indirectDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
#define RE_Direct				RE_Direct_BlinnPhong
#define RE_IndirectDiffuse		RE_IndirectDiffuse_BlinnPhong`,lights_physical_fragment:`PhysicalMaterial material;
material.diffuseColor = diffuseColor.rgb * ( 1.0 - metalnessFactor );
vec3 dxy = max( abs( dFdx( nonPerturbedNormal ) ), abs( dFdy( nonPerturbedNormal ) ) );
float geometryRoughness = max( max( dxy.x, dxy.y ), dxy.z );
material.roughness = max( roughnessFactor, 0.0525 );material.roughness += geometryRoughness;
material.roughness = min( material.roughness, 1.0 );
#ifdef IOR
	material.ior = ior;
	#ifdef USE_SPECULAR
		float specularIntensityFactor = specularIntensity;
		vec3 specularColorFactor = specularColor;
		#ifdef USE_SPECULAR_COLORMAP
			specularColorFactor *= texture2D( specularColorMap, vSpecularColorMapUv ).rgb;
		#endif
		#ifdef USE_SPECULAR_INTENSITYMAP
			specularIntensityFactor *= texture2D( specularIntensityMap, vSpecularIntensityMapUv ).a;
		#endif
		material.specularF90 = mix( specularIntensityFactor, 1.0, metalnessFactor );
	#else
		float specularIntensityFactor = 1.0;
		vec3 specularColorFactor = vec3( 1.0 );
		material.specularF90 = 1.0;
	#endif
	material.specularColor = mix( min( pow2( ( material.ior - 1.0 ) / ( material.ior + 1.0 ) ) * specularColorFactor, vec3( 1.0 ) ) * specularIntensityFactor, diffuseColor.rgb, metalnessFactor );
#else
	material.specularColor = mix( vec3( 0.04 ), diffuseColor.rgb, metalnessFactor );
	material.specularF90 = 1.0;
#endif
#ifdef USE_CLEARCOAT
	material.clearcoat = clearcoat;
	material.clearcoatRoughness = clearcoatRoughness;
	material.clearcoatF0 = vec3( 0.04 );
	material.clearcoatF90 = 1.0;
	#ifdef USE_CLEARCOATMAP
		material.clearcoat *= texture2D( clearcoatMap, vClearcoatMapUv ).x;
	#endif
	#ifdef USE_CLEARCOAT_ROUGHNESSMAP
		material.clearcoatRoughness *= texture2D( clearcoatRoughnessMap, vClearcoatRoughnessMapUv ).y;
	#endif
	material.clearcoat = saturate( material.clearcoat );	material.clearcoatRoughness = max( material.clearcoatRoughness, 0.0525 );
	material.clearcoatRoughness += geometryRoughness;
	material.clearcoatRoughness = min( material.clearcoatRoughness, 1.0 );
#endif
#ifdef USE_IRIDESCENCE
	material.iridescence = iridescence;
	material.iridescenceIOR = iridescenceIOR;
	#ifdef USE_IRIDESCENCEMAP
		material.iridescence *= texture2D( iridescenceMap, vIridescenceMapUv ).r;
	#endif
	#ifdef USE_IRIDESCENCE_THICKNESSMAP
		material.iridescenceThickness = (iridescenceThicknessMaximum - iridescenceThicknessMinimum) * texture2D( iridescenceThicknessMap, vIridescenceThicknessMapUv ).g + iridescenceThicknessMinimum;
	#else
		material.iridescenceThickness = iridescenceThicknessMaximum;
	#endif
#endif
#ifdef USE_SHEEN
	material.sheenColor = sheenColor;
	#ifdef USE_SHEEN_COLORMAP
		material.sheenColor *= texture2D( sheenColorMap, vSheenColorMapUv ).rgb;
	#endif
	material.sheenRoughness = clamp( sheenRoughness, 0.07, 1.0 );
	#ifdef USE_SHEEN_ROUGHNESSMAP
		material.sheenRoughness *= texture2D( sheenRoughnessMap, vSheenRoughnessMapUv ).a;
	#endif
#endif
#ifdef USE_ANISOTROPY
	#ifdef USE_ANISOTROPYMAP
		mat2 anisotropyMat = mat2( anisotropyVector.x, anisotropyVector.y, - anisotropyVector.y, anisotropyVector.x );
		vec3 anisotropyPolar = texture2D( anisotropyMap, vAnisotropyMapUv ).rgb;
		vec2 anisotropyV = anisotropyMat * normalize( 2.0 * anisotropyPolar.rg - vec2( 1.0 ) ) * anisotropyPolar.b;
	#else
		vec2 anisotropyV = anisotropyVector;
	#endif
	material.anisotropy = length( anisotropyV );
	if( material.anisotropy == 0.0 ) {
		anisotropyV = vec2( 1.0, 0.0 );
	} else {
		anisotropyV /= material.anisotropy;
		material.anisotropy = saturate( material.anisotropy );
	}
	material.alphaT = mix( pow2( material.roughness ), 1.0, pow2( material.anisotropy ) );
	material.anisotropyT = tbn[ 0 ] * anisotropyV.x + tbn[ 1 ] * anisotropyV.y;
	material.anisotropyB = tbn[ 1 ] * anisotropyV.x - tbn[ 0 ] * anisotropyV.y;
#endif`,lights_physical_pars_fragment:`struct PhysicalMaterial {
	vec3 diffuseColor;
	float roughness;
	vec3 specularColor;
	float specularF90;
	#ifdef USE_CLEARCOAT
		float clearcoat;
		float clearcoatRoughness;
		vec3 clearcoatF0;
		float clearcoatF90;
	#endif
	#ifdef USE_IRIDESCENCE
		float iridescence;
		float iridescenceIOR;
		float iridescenceThickness;
		vec3 iridescenceFresnel;
		vec3 iridescenceF0;
	#endif
	#ifdef USE_SHEEN
		vec3 sheenColor;
		float sheenRoughness;
	#endif
	#ifdef IOR
		float ior;
	#endif
	#ifdef USE_TRANSMISSION
		float transmission;
		float transmissionAlpha;
		float thickness;
		float attenuationDistance;
		vec3 attenuationColor;
	#endif
	#ifdef USE_ANISOTROPY
		float anisotropy;
		float alphaT;
		vec3 anisotropyT;
		vec3 anisotropyB;
	#endif
};
vec3 clearcoatSpecularDirect = vec3( 0.0 );
vec3 clearcoatSpecularIndirect = vec3( 0.0 );
vec3 sheenSpecularDirect = vec3( 0.0 );
vec3 sheenSpecularIndirect = vec3(0.0 );
vec3 Schlick_to_F0( const in vec3 f, const in float f90, const in float dotVH ) {
    float x = clamp( 1.0 - dotVH, 0.0, 1.0 );
    float x2 = x * x;
    float x5 = clamp( x * x2 * x2, 0.0, 0.9999 );
    return ( f - vec3( f90 ) * x5 ) / ( 1.0 - x5 );
}
float V_GGX_SmithCorrelated( const in float alpha, const in float dotNL, const in float dotNV ) {
	float a2 = pow2( alpha );
	float gv = dotNL * sqrt( a2 + ( 1.0 - a2 ) * pow2( dotNV ) );
	float gl = dotNV * sqrt( a2 + ( 1.0 - a2 ) * pow2( dotNL ) );
	return 0.5 / max( gv + gl, EPSILON );
}
float D_GGX( const in float alpha, const in float dotNH ) {
	float a2 = pow2( alpha );
	float denom = pow2( dotNH ) * ( a2 - 1.0 ) + 1.0;
	return RECIPROCAL_PI * a2 / pow2( denom );
}
#ifdef USE_ANISOTROPY
	float V_GGX_SmithCorrelated_Anisotropic( const in float alphaT, const in float alphaB, const in float dotTV, const in float dotBV, const in float dotTL, const in float dotBL, const in float dotNV, const in float dotNL ) {
		float gv = dotNL * length( vec3( alphaT * dotTV, alphaB * dotBV, dotNV ) );
		float gl = dotNV * length( vec3( alphaT * dotTL, alphaB * dotBL, dotNL ) );
		float v = 0.5 / ( gv + gl );
		return saturate(v);
	}
	float D_GGX_Anisotropic( const in float alphaT, const in float alphaB, const in float dotNH, const in float dotTH, const in float dotBH ) {
		float a2 = alphaT * alphaB;
		highp vec3 v = vec3( alphaB * dotTH, alphaT * dotBH, a2 * dotNH );
		highp float v2 = dot( v, v );
		float w2 = a2 / v2;
		return RECIPROCAL_PI * a2 * pow2 ( w2 );
	}
#endif
#ifdef USE_CLEARCOAT
	vec3 BRDF_GGX_Clearcoat( const in vec3 lightDir, const in vec3 viewDir, const in vec3 normal, const in PhysicalMaterial material) {
		vec3 f0 = material.clearcoatF0;
		float f90 = material.clearcoatF90;
		float roughness = material.clearcoatRoughness;
		float alpha = pow2( roughness );
		vec3 halfDir = normalize( lightDir + viewDir );
		float dotNL = saturate( dot( normal, lightDir ) );
		float dotNV = saturate( dot( normal, viewDir ) );
		float dotNH = saturate( dot( normal, halfDir ) );
		float dotVH = saturate( dot( viewDir, halfDir ) );
		vec3 F = F_Schlick( f0, f90, dotVH );
		float V = V_GGX_SmithCorrelated( alpha, dotNL, dotNV );
		float D = D_GGX( alpha, dotNH );
		return F * ( V * D );
	}
#endif
vec3 BRDF_GGX( const in vec3 lightDir, const in vec3 viewDir, const in vec3 normal, const in PhysicalMaterial material ) {
	vec3 f0 = material.specularColor;
	float f90 = material.specularF90;
	float roughness = material.roughness;
	float alpha = pow2( roughness );
	vec3 halfDir = normalize( lightDir + viewDir );
	float dotNL = saturate( dot( normal, lightDir ) );
	float dotNV = saturate( dot( normal, viewDir ) );
	float dotNH = saturate( dot( normal, halfDir ) );
	float dotVH = saturate( dot( viewDir, halfDir ) );
	vec3 F = F_Schlick( f0, f90, dotVH );
	#ifdef USE_IRIDESCENCE
		F = mix( F, material.iridescenceFresnel, material.iridescence );
	#endif
	#ifdef USE_ANISOTROPY
		float dotTL = dot( material.anisotropyT, lightDir );
		float dotTV = dot( material.anisotropyT, viewDir );
		float dotTH = dot( material.anisotropyT, halfDir );
		float dotBL = dot( material.anisotropyB, lightDir );
		float dotBV = dot( material.anisotropyB, viewDir );
		float dotBH = dot( material.anisotropyB, halfDir );
		float V = V_GGX_SmithCorrelated_Anisotropic( material.alphaT, alpha, dotTV, dotBV, dotTL, dotBL, dotNV, dotNL );
		float D = D_GGX_Anisotropic( material.alphaT, alpha, dotNH, dotTH, dotBH );
	#else
		float V = V_GGX_SmithCorrelated( alpha, dotNL, dotNV );
		float D = D_GGX( alpha, dotNH );
	#endif
	return F * ( V * D );
}
vec2 LTC_Uv( const in vec3 N, const in vec3 V, const in float roughness ) {
	const float LUT_SIZE = 64.0;
	const float LUT_SCALE = ( LUT_SIZE - 1.0 ) / LUT_SIZE;
	const float LUT_BIAS = 0.5 / LUT_SIZE;
	float dotNV = saturate( dot( N, V ) );
	vec2 uv = vec2( roughness, sqrt( 1.0 - dotNV ) );
	uv = uv * LUT_SCALE + LUT_BIAS;
	return uv;
}
float LTC_ClippedSphereFormFactor( const in vec3 f ) {
	float l = length( f );
	return max( ( l * l + f.z ) / ( l + 1.0 ), 0.0 );
}
vec3 LTC_EdgeVectorFormFactor( const in vec3 v1, const in vec3 v2 ) {
	float x = dot( v1, v2 );
	float y = abs( x );
	float a = 0.8543985 + ( 0.4965155 + 0.0145206 * y ) * y;
	float b = 3.4175940 + ( 4.1616724 + y ) * y;
	float v = a / b;
	float theta_sintheta = ( x > 0.0 ) ? v : 0.5 * inversesqrt( max( 1.0 - x * x, 1e-7 ) ) - v;
	return cross( v1, v2 ) * theta_sintheta;
}
vec3 LTC_Evaluate( const in vec3 N, const in vec3 V, const in vec3 P, const in mat3 mInv, const in vec3 rectCoords[ 4 ] ) {
	vec3 v1 = rectCoords[ 1 ] - rectCoords[ 0 ];
	vec3 v2 = rectCoords[ 3 ] - rectCoords[ 0 ];
	vec3 lightNormal = cross( v1, v2 );
	if( dot( lightNormal, P - rectCoords[ 0 ] ) < 0.0 ) return vec3( 0.0 );
	vec3 T1, T2;
	T1 = normalize( V - N * dot( V, N ) );
	T2 = - cross( N, T1 );
	mat3 mat = mInv * transposeMat3( mat3( T1, T2, N ) );
	vec3 coords[ 4 ];
	coords[ 0 ] = mat * ( rectCoords[ 0 ] - P );
	coords[ 1 ] = mat * ( rectCoords[ 1 ] - P );
	coords[ 2 ] = mat * ( rectCoords[ 2 ] - P );
	coords[ 3 ] = mat * ( rectCoords[ 3 ] - P );
	coords[ 0 ] = normalize( coords[ 0 ] );
	coords[ 1 ] = normalize( coords[ 1 ] );
	coords[ 2 ] = normalize( coords[ 2 ] );
	coords[ 3 ] = normalize( coords[ 3 ] );
	vec3 vectorFormFactor = vec3( 0.0 );
	vectorFormFactor += LTC_EdgeVectorFormFactor( coords[ 0 ], coords[ 1 ] );
	vectorFormFactor += LTC_EdgeVectorFormFactor( coords[ 1 ], coords[ 2 ] );
	vectorFormFactor += LTC_EdgeVectorFormFactor( coords[ 2 ], coords[ 3 ] );
	vectorFormFactor += LTC_EdgeVectorFormFactor( coords[ 3 ], coords[ 0 ] );
	float result = LTC_ClippedSphereFormFactor( vectorFormFactor );
	return vec3( result );
}
#if defined( USE_SHEEN )
float D_Charlie( float roughness, float dotNH ) {
	float alpha = pow2( roughness );
	float invAlpha = 1.0 / alpha;
	float cos2h = dotNH * dotNH;
	float sin2h = max( 1.0 - cos2h, 0.0078125 );
	return ( 2.0 + invAlpha ) * pow( sin2h, invAlpha * 0.5 ) / ( 2.0 * PI );
}
float V_Neubelt( float dotNV, float dotNL ) {
	return saturate( 1.0 / ( 4.0 * ( dotNL + dotNV - dotNL * dotNV ) ) );
}
vec3 BRDF_Sheen( const in vec3 lightDir, const in vec3 viewDir, const in vec3 normal, vec3 sheenColor, const in float sheenRoughness ) {
	vec3 halfDir = normalize( lightDir + viewDir );
	float dotNL = saturate( dot( normal, lightDir ) );
	float dotNV = saturate( dot( normal, viewDir ) );
	float dotNH = saturate( dot( normal, halfDir ) );
	float D = D_Charlie( sheenRoughness, dotNH );
	float V = V_Neubelt( dotNV, dotNL );
	return sheenColor * ( D * V );
}
#endif
float IBLSheenBRDF( const in vec3 normal, const in vec3 viewDir, const in float roughness ) {
	float dotNV = saturate( dot( normal, viewDir ) );
	float r2 = roughness * roughness;
	float a = roughness < 0.25 ? -339.2 * r2 + 161.4 * roughness - 25.9 : -8.48 * r2 + 14.3 * roughness - 9.95;
	float b = roughness < 0.25 ? 44.0 * r2 - 23.7 * roughness + 3.26 : 1.97 * r2 - 3.27 * roughness + 0.72;
	float DG = exp( a * dotNV + b ) + ( roughness < 0.25 ? 0.0 : 0.1 * ( roughness - 0.25 ) );
	return saturate( DG * RECIPROCAL_PI );
}
vec2 DFGApprox( const in vec3 normal, const in vec3 viewDir, const in float roughness ) {
	float dotNV = saturate( dot( normal, viewDir ) );
	const vec4 c0 = vec4( - 1, - 0.0275, - 0.572, 0.022 );
	const vec4 c1 = vec4( 1, 0.0425, 1.04, - 0.04 );
	vec4 r = roughness * c0 + c1;
	float a004 = min( r.x * r.x, exp2( - 9.28 * dotNV ) ) * r.x + r.y;
	vec2 fab = vec2( - 1.04, 1.04 ) * a004 + r.zw;
	return fab;
}
vec3 EnvironmentBRDF( const in vec3 normal, const in vec3 viewDir, const in vec3 specularColor, const in float specularF90, const in float roughness ) {
	vec2 fab = DFGApprox( normal, viewDir, roughness );
	return specularColor * fab.x + specularF90 * fab.y;
}
#ifdef USE_IRIDESCENCE
void computeMultiscatteringIridescence( const in vec3 normal, const in vec3 viewDir, const in vec3 specularColor, const in float specularF90, const in float iridescence, const in vec3 iridescenceF0, const in float roughness, inout vec3 singleScatter, inout vec3 multiScatter ) {
#else
void computeMultiscattering( const in vec3 normal, const in vec3 viewDir, const in vec3 specularColor, const in float specularF90, const in float roughness, inout vec3 singleScatter, inout vec3 multiScatter ) {
#endif
	vec2 fab = DFGApprox( normal, viewDir, roughness );
	#ifdef USE_IRIDESCENCE
		vec3 Fr = mix( specularColor, iridescenceF0, iridescence );
	#else
		vec3 Fr = specularColor;
	#endif
	vec3 FssEss = Fr * fab.x + specularF90 * fab.y;
	float Ess = fab.x + fab.y;
	float Ems = 1.0 - Ess;
	vec3 Favg = Fr + ( 1.0 - Fr ) * 0.047619;	vec3 Fms = FssEss * Favg / ( 1.0 - Ems * Favg );
	singleScatter += FssEss;
	multiScatter += Fms * Ems;
}
#if NUM_RECT_AREA_LIGHTS > 0
	void RE_Direct_RectArea_Physical( const in RectAreaLight rectAreaLight, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in PhysicalMaterial material, inout ReflectedLight reflectedLight ) {
		vec3 normal = geometryNormal;
		vec3 viewDir = geometryViewDir;
		vec3 position = geometryPosition;
		vec3 lightPos = rectAreaLight.position;
		vec3 halfWidth = rectAreaLight.halfWidth;
		vec3 halfHeight = rectAreaLight.halfHeight;
		vec3 lightColor = rectAreaLight.color;
		float roughness = material.roughness;
		vec3 rectCoords[ 4 ];
		rectCoords[ 0 ] = lightPos + halfWidth - halfHeight;		rectCoords[ 1 ] = lightPos - halfWidth - halfHeight;
		rectCoords[ 2 ] = lightPos - halfWidth + halfHeight;
		rectCoords[ 3 ] = lightPos + halfWidth + halfHeight;
		vec2 uv = LTC_Uv( normal, viewDir, roughness );
		vec4 t1 = texture2D( ltc_1, uv );
		vec4 t2 = texture2D( ltc_2, uv );
		mat3 mInv = mat3(
			vec3( t1.x, 0, t1.y ),
			vec3(    0, 1,    0 ),
			vec3( t1.z, 0, t1.w )
		);
		vec3 fresnel = ( material.specularColor * t2.x + ( vec3( 1.0 ) - material.specularColor ) * t2.y );
		reflectedLight.directSpecular += lightColor * fresnel * LTC_Evaluate( normal, viewDir, position, mInv, rectCoords );
		reflectedLight.directDiffuse += lightColor * material.diffuseColor * LTC_Evaluate( normal, viewDir, position, mat3( 1.0 ), rectCoords );
	}
#endif
void RE_Direct_Physical( const in IncidentLight directLight, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in PhysicalMaterial material, inout ReflectedLight reflectedLight ) {
	float dotNL = saturate( dot( geometryNormal, directLight.direction ) );
	vec3 irradiance = dotNL * directLight.color;
	#ifdef USE_CLEARCOAT
		float dotNLcc = saturate( dot( geometryClearcoatNormal, directLight.direction ) );
		vec3 ccIrradiance = dotNLcc * directLight.color;
		clearcoatSpecularDirect += ccIrradiance * BRDF_GGX_Clearcoat( directLight.direction, geometryViewDir, geometryClearcoatNormal, material );
	#endif
	#ifdef USE_SHEEN
		sheenSpecularDirect += irradiance * BRDF_Sheen( directLight.direction, geometryViewDir, geometryNormal, material.sheenColor, material.sheenRoughness );
	#endif
	reflectedLight.directSpecular += irradiance * BRDF_GGX( directLight.direction, geometryViewDir, geometryNormal, material );
	reflectedLight.directDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
void RE_IndirectDiffuse_Physical( const in vec3 irradiance, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in PhysicalMaterial material, inout ReflectedLight reflectedLight ) {
	reflectedLight.indirectDiffuse += irradiance * BRDF_Lambert( material.diffuseColor );
}
void RE_IndirectSpecular_Physical( const in vec3 radiance, const in vec3 irradiance, const in vec3 clearcoatRadiance, const in vec3 geometryPosition, const in vec3 geometryNormal, const in vec3 geometryViewDir, const in vec3 geometryClearcoatNormal, const in PhysicalMaterial material, inout ReflectedLight reflectedLight) {
	#ifdef USE_CLEARCOAT
		clearcoatSpecularIndirect += clearcoatRadiance * EnvironmentBRDF( geometryClearcoatNormal, geometryViewDir, material.clearcoatF0, material.clearcoatF90, material.clearcoatRoughness );
	#endif
	#ifdef USE_SHEEN
		sheenSpecularIndirect += irradiance * material.sheenColor * IBLSheenBRDF( geometryNormal, geometryViewDir, material.sheenRoughness );
	#endif
	vec3 singleScattering = vec3( 0.0 );
	vec3 multiScattering = vec3( 0.0 );
	vec3 cosineWeightedIrradiance = irradiance * RECIPROCAL_PI;
	#ifdef USE_IRIDESCENCE
		computeMultiscatteringIridescence( geometryNormal, geometryViewDir, material.specularColor, material.specularF90, material.iridescence, material.iridescenceFresnel, material.roughness, singleScattering, multiScattering );
	#else
		computeMultiscattering( geometryNormal, geometryViewDir, material.specularColor, material.specularF90, material.roughness, singleScattering, multiScattering );
	#endif
	vec3 totalScattering = singleScattering + multiScattering;
	vec3 diffuse = material.diffuseColor * ( 1.0 - max( max( totalScattering.r, totalScattering.g ), totalScattering.b ) );
	reflectedLight.indirectSpecular += radiance * singleScattering;
	reflectedLight.indirectSpecular += multiScattering * cosineWeightedIrradiance;
	reflectedLight.indirectDiffuse += diffuse * cosineWeightedIrradiance;
}
#define RE_Direct				RE_Direct_Physical
#define RE_Direct_RectArea		RE_Direct_RectArea_Physical
#define RE_IndirectDiffuse		RE_IndirectDiffuse_Physical
#define RE_IndirectSpecular		RE_IndirectSpecular_Physical
float computeSpecularOcclusion( const in float dotNV, const in float ambientOcclusion, const in float roughness ) {
	return saturate( pow( dotNV + ambientOcclusion, exp2( - 16.0 * roughness - 1.0 ) ) - 1.0 + ambientOcclusion );
}`,lights_fragment_begin:`
vec3 geometryPosition = - vViewPosition;
vec3 geometryNormal = normal;
vec3 geometryViewDir = ( isOrthographic ) ? vec3( 0, 0, 1 ) : normalize( vViewPosition );
vec3 geometryClearcoatNormal = vec3( 0.0 );
#ifdef USE_CLEARCOAT
	geometryClearcoatNormal = clearcoatNormal;
#endif
#ifdef USE_IRIDESCENCE
	float dotNVi = saturate( dot( normal, geometryViewDir ) );
	if ( material.iridescenceThickness == 0.0 ) {
		material.iridescence = 0.0;
	} else {
		material.iridescence = saturate( material.iridescence );
	}
	if ( material.iridescence > 0.0 ) {
		material.iridescenceFresnel = evalIridescence( 1.0, material.iridescenceIOR, dotNVi, material.iridescenceThickness, material.specularColor );
		material.iridescenceF0 = Schlick_to_F0( material.iridescenceFresnel, 1.0, dotNVi );
	}
#endif
IncidentLight directLight;
#if ( NUM_POINT_LIGHTS > 0 ) && defined( RE_Direct )
	PointLight pointLight;
	#if defined( USE_SHADOWMAP ) && NUM_POINT_LIGHT_SHADOWS > 0
	PointLightShadow pointLightShadow;
	#endif
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_POINT_LIGHTS; i ++ ) {
		pointLight = pointLights[ i ];
		getPointLightInfo( pointLight, geometryPosition, directLight );
		#if defined( USE_SHADOWMAP ) && ( UNROLLED_LOOP_INDEX < NUM_POINT_LIGHT_SHADOWS )
		pointLightShadow = pointLightShadows[ i ];
		directLight.color *= ( directLight.visible && receiveShadow ) ? getPointShadow( pointShadowMap[ i ], pointLightShadow.shadowMapSize, pointLightShadow.shadowBias, pointLightShadow.shadowRadius, vPointShadowCoord[ i ], pointLightShadow.shadowCameraNear, pointLightShadow.shadowCameraFar ) : 1.0;
		#endif
		RE_Direct( directLight, geometryPosition, geometryNormal, geometryViewDir, geometryClearcoatNormal, material, reflectedLight );
	}
	#pragma unroll_loop_end
#endif
#if ( NUM_SPOT_LIGHTS > 0 ) && defined( RE_Direct )
	SpotLight spotLight;
	vec4 spotColor;
	vec3 spotLightCoord;
	bool inSpotLightMap;
	#if defined( USE_SHADOWMAP ) && NUM_SPOT_LIGHT_SHADOWS > 0
	SpotLightShadow spotLightShadow;
	#endif
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_SPOT_LIGHTS; i ++ ) {
		spotLight = spotLights[ i ];
		getSpotLightInfo( spotLight, geometryPosition, directLight );
		#if ( UNROLLED_LOOP_INDEX < NUM_SPOT_LIGHT_SHADOWS_WITH_MAPS )
		#define SPOT_LIGHT_MAP_INDEX UNROLLED_LOOP_INDEX
		#elif ( UNROLLED_LOOP_INDEX < NUM_SPOT_LIGHT_SHADOWS )
		#define SPOT_LIGHT_MAP_INDEX NUM_SPOT_LIGHT_MAPS
		#else
		#define SPOT_LIGHT_MAP_INDEX ( UNROLLED_LOOP_INDEX - NUM_SPOT_LIGHT_SHADOWS + NUM_SPOT_LIGHT_SHADOWS_WITH_MAPS )
		#endif
		#if ( SPOT_LIGHT_MAP_INDEX < NUM_SPOT_LIGHT_MAPS )
			spotLightCoord = vSpotLightCoord[ i ].xyz / vSpotLightCoord[ i ].w;
			inSpotLightMap = all( lessThan( abs( spotLightCoord * 2. - 1. ), vec3( 1.0 ) ) );
			spotColor = texture2D( spotLightMap[ SPOT_LIGHT_MAP_INDEX ], spotLightCoord.xy );
			directLight.color = inSpotLightMap ? directLight.color * spotColor.rgb : directLight.color;
		#endif
		#undef SPOT_LIGHT_MAP_INDEX
		#if defined( USE_SHADOWMAP ) && ( UNROLLED_LOOP_INDEX < NUM_SPOT_LIGHT_SHADOWS )
		spotLightShadow = spotLightShadows[ i ];
		directLight.color *= ( directLight.visible && receiveShadow ) ? getShadow( spotShadowMap[ i ], spotLightShadow.shadowMapSize, spotLightShadow.shadowBias, spotLightShadow.shadowRadius, vSpotLightCoord[ i ] ) : 1.0;
		#endif
		RE_Direct( directLight, geometryPosition, geometryNormal, geometryViewDir, geometryClearcoatNormal, material, reflectedLight );
	}
	#pragma unroll_loop_end
#endif
#if ( NUM_DIR_LIGHTS > 0 ) && defined( RE_Direct )
	DirectionalLight directionalLight;
	#if defined( USE_SHADOWMAP ) && NUM_DIR_LIGHT_SHADOWS > 0
	DirectionalLightShadow directionalLightShadow;
	#endif
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_DIR_LIGHTS; i ++ ) {
		directionalLight = directionalLights[ i ];
		getDirectionalLightInfo( directionalLight, directLight );
		#if defined( USE_SHADOWMAP ) && ( UNROLLED_LOOP_INDEX < NUM_DIR_LIGHT_SHADOWS )
		directionalLightShadow = directionalLightShadows[ i ];
		directLight.color *= ( directLight.visible && receiveShadow ) ? getShadow( directionalShadowMap[ i ], directionalLightShadow.shadowMapSize, directionalLightShadow.shadowBias, directionalLightShadow.shadowRadius, vDirectionalShadowCoord[ i ] ) : 1.0;
		#endif
		RE_Direct( directLight, geometryPosition, geometryNormal, geometryViewDir, geometryClearcoatNormal, material, reflectedLight );
	}
	#pragma unroll_loop_end
#endif
#if ( NUM_RECT_AREA_LIGHTS > 0 ) && defined( RE_Direct_RectArea )
	RectAreaLight rectAreaLight;
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_RECT_AREA_LIGHTS; i ++ ) {
		rectAreaLight = rectAreaLights[ i ];
		RE_Direct_RectArea( rectAreaLight, geometryPosition, geometryNormal, geometryViewDir, geometryClearcoatNormal, material, reflectedLight );
	}
	#pragma unroll_loop_end
#endif
#if defined( RE_IndirectDiffuse )
	vec3 iblIrradiance = vec3( 0.0 );
	vec3 irradiance = getAmbientLightIrradiance( ambientLightColor );
	#if defined( USE_LIGHT_PROBES )
		irradiance += getLightProbeIrradiance( lightProbe, geometryNormal );
	#endif
	#if ( NUM_HEMI_LIGHTS > 0 )
		#pragma unroll_loop_start
		for ( int i = 0; i < NUM_HEMI_LIGHTS; i ++ ) {
			irradiance += getHemisphereLightIrradiance( hemisphereLights[ i ], geometryNormal );
		}
		#pragma unroll_loop_end
	#endif
#endif
#if defined( RE_IndirectSpecular )
	vec3 radiance = vec3( 0.0 );
	vec3 clearcoatRadiance = vec3( 0.0 );
#endif`,lights_fragment_maps:`#if defined( RE_IndirectDiffuse )
	#ifdef USE_LIGHTMAP
		vec4 lightMapTexel = texture2D( lightMap, vLightMapUv );
		vec3 lightMapIrradiance = lightMapTexel.rgb * lightMapIntensity;
		irradiance += lightMapIrradiance;
	#endif
	#if defined( USE_ENVMAP ) && defined( STANDARD ) && defined( ENVMAP_TYPE_CUBE_UV )
		iblIrradiance += getIBLIrradiance( geometryNormal );
	#endif
#endif
#if defined( USE_ENVMAP ) && defined( RE_IndirectSpecular )
	#ifdef USE_ANISOTROPY
		radiance += getIBLAnisotropyRadiance( geometryViewDir, geometryNormal, material.roughness, material.anisotropyB, material.anisotropy );
	#else
		radiance += getIBLRadiance( geometryViewDir, geometryNormal, material.roughness );
	#endif
	#ifdef USE_CLEARCOAT
		clearcoatRadiance += getIBLRadiance( geometryViewDir, geometryClearcoatNormal, material.clearcoatRoughness );
	#endif
#endif`,lights_fragment_end:`#if defined( RE_IndirectDiffuse )
	RE_IndirectDiffuse( irradiance, geometryPosition, geometryNormal, geometryViewDir, geometryClearcoatNormal, material, reflectedLight );
#endif
#if defined( RE_IndirectSpecular )
	RE_IndirectSpecular( radiance, iblIrradiance, clearcoatRadiance, geometryPosition, geometryNormal, geometryViewDir, geometryClearcoatNormal, material, reflectedLight );
#endif`,logdepthbuf_fragment:`#if defined( USE_LOGDEPTHBUF ) && defined( USE_LOGDEPTHBUF_EXT )
	gl_FragDepthEXT = vIsPerspective == 0.0 ? gl_FragCoord.z : log2( vFragDepth ) * logDepthBufFC * 0.5;
#endif`,logdepthbuf_pars_fragment:`#if defined( USE_LOGDEPTHBUF ) && defined( USE_LOGDEPTHBUF_EXT )
	uniform float logDepthBufFC;
	varying float vFragDepth;
	varying float vIsPerspective;
#endif`,logdepthbuf_pars_vertex:`#ifdef USE_LOGDEPTHBUF
	#ifdef USE_LOGDEPTHBUF_EXT
		varying float vFragDepth;
		varying float vIsPerspective;
	#else
		uniform float logDepthBufFC;
	#endif
#endif`,logdepthbuf_vertex:`#ifdef USE_LOGDEPTHBUF
	#ifdef USE_LOGDEPTHBUF_EXT
		vFragDepth = 1.0 + gl_Position.w;
		vIsPerspective = float( isPerspectiveMatrix( projectionMatrix ) );
	#else
		if ( isPerspectiveMatrix( projectionMatrix ) ) {
			gl_Position.z = log2( max( EPSILON, gl_Position.w + 1.0 ) ) * logDepthBufFC - 1.0;
			gl_Position.z *= gl_Position.w;
		}
	#endif
#endif`,map_fragment:`#ifdef USE_MAP
	vec4 sampledDiffuseColor = texture2D( map, vMapUv );
	#ifdef DECODE_VIDEO_TEXTURE
		sampledDiffuseColor = vec4( mix( pow( sampledDiffuseColor.rgb * 0.9478672986 + vec3( 0.0521327014 ), vec3( 2.4 ) ), sampledDiffuseColor.rgb * 0.0773993808, vec3( lessThanEqual( sampledDiffuseColor.rgb, vec3( 0.04045 ) ) ) ), sampledDiffuseColor.w );
	
	#endif
	diffuseColor *= sampledDiffuseColor;
#endif`,map_pars_fragment:`#ifdef USE_MAP
	uniform sampler2D map;
#endif`,map_particle_fragment:`#if defined( USE_MAP ) || defined( USE_ALPHAMAP )
	#if defined( USE_POINTS_UV )
		vec2 uv = vUv;
	#else
		vec2 uv = ( uvTransform * vec3( gl_PointCoord.x, 1.0 - gl_PointCoord.y, 1 ) ).xy;
	#endif
#endif
#ifdef USE_MAP
	diffuseColor *= texture2D( map, uv );
#endif
#ifdef USE_ALPHAMAP
	diffuseColor.a *= texture2D( alphaMap, uv ).g;
#endif`,map_particle_pars_fragment:`#if defined( USE_POINTS_UV )
	varying vec2 vUv;
#else
	#if defined( USE_MAP ) || defined( USE_ALPHAMAP )
		uniform mat3 uvTransform;
	#endif
#endif
#ifdef USE_MAP
	uniform sampler2D map;
#endif
#ifdef USE_ALPHAMAP
	uniform sampler2D alphaMap;
#endif`,metalnessmap_fragment:`float metalnessFactor = metalness;
#ifdef USE_METALNESSMAP
	vec4 texelMetalness = texture2D( metalnessMap, vMetalnessMapUv );
	metalnessFactor *= texelMetalness.b;
#endif`,metalnessmap_pars_fragment:`#ifdef USE_METALNESSMAP
	uniform sampler2D metalnessMap;
#endif`,morphinstance_vertex:`#ifdef USE_INSTANCING_MORPH
	float morphTargetInfluences[MORPHTARGETS_COUNT];
	float morphTargetBaseInfluence = texelFetch( morphTexture, ivec2( 0, gl_InstanceID ), 0 ).r;
	for ( int i = 0; i < MORPHTARGETS_COUNT; i ++ ) {
		morphTargetInfluences[i] =  texelFetch( morphTexture, ivec2( i + 1, gl_InstanceID ), 0 ).r;
	}
#endif`,morphcolor_vertex:`#if defined( USE_MORPHCOLORS ) && defined( MORPHTARGETS_TEXTURE )
	vColor *= morphTargetBaseInfluence;
	for ( int i = 0; i < MORPHTARGETS_COUNT; i ++ ) {
		#if defined( USE_COLOR_ALPHA )
			if ( morphTargetInfluences[ i ] != 0.0 ) vColor += getMorph( gl_VertexID, i, 2 ) * morphTargetInfluences[ i ];
		#elif defined( USE_COLOR )
			if ( morphTargetInfluences[ i ] != 0.0 ) vColor += getMorph( gl_VertexID, i, 2 ).rgb * morphTargetInfluences[ i ];
		#endif
	}
#endif`,morphnormal_vertex:`#ifdef USE_MORPHNORMALS
	objectNormal *= morphTargetBaseInfluence;
	#ifdef MORPHTARGETS_TEXTURE
		for ( int i = 0; i < MORPHTARGETS_COUNT; i ++ ) {
			if ( morphTargetInfluences[ i ] != 0.0 ) objectNormal += getMorph( gl_VertexID, i, 1 ).xyz * morphTargetInfluences[ i ];
		}
	#else
		objectNormal += morphNormal0 * morphTargetInfluences[ 0 ];
		objectNormal += morphNormal1 * morphTargetInfluences[ 1 ];
		objectNormal += morphNormal2 * morphTargetInfluences[ 2 ];
		objectNormal += morphNormal3 * morphTargetInfluences[ 3 ];
	#endif
#endif`,morphtarget_pars_vertex:`#ifdef USE_MORPHTARGETS
	#ifndef USE_INSTANCING_MORPH
		uniform float morphTargetBaseInfluence;
	#endif
	#ifdef MORPHTARGETS_TEXTURE
		#ifndef USE_INSTANCING_MORPH
			uniform float morphTargetInfluences[ MORPHTARGETS_COUNT ];
		#endif
		uniform sampler2DArray morphTargetsTexture;
		uniform ivec2 morphTargetsTextureSize;
		vec4 getMorph( const in int vertexIndex, const in int morphTargetIndex, const in int offset ) {
			int texelIndex = vertexIndex * MORPHTARGETS_TEXTURE_STRIDE + offset;
			int y = texelIndex / morphTargetsTextureSize.x;
			int x = texelIndex - y * morphTargetsTextureSize.x;
			ivec3 morphUV = ivec3( x, y, morphTargetIndex );
			return texelFetch( morphTargetsTexture, morphUV, 0 );
		}
	#else
		#ifndef USE_MORPHNORMALS
			uniform float morphTargetInfluences[ 8 ];
		#else
			uniform float morphTargetInfluences[ 4 ];
		#endif
	#endif
#endif`,morphtarget_vertex:`#ifdef USE_MORPHTARGETS
	transformed *= morphTargetBaseInfluence;
	#ifdef MORPHTARGETS_TEXTURE
		for ( int i = 0; i < MORPHTARGETS_COUNT; i ++ ) {
			if ( morphTargetInfluences[ i ] != 0.0 ) transformed += getMorph( gl_VertexID, i, 0 ).xyz * morphTargetInfluences[ i ];
		}
	#else
		transformed += morphTarget0 * morphTargetInfluences[ 0 ];
		transformed += morphTarget1 * morphTargetInfluences[ 1 ];
		transformed += morphTarget2 * morphTargetInfluences[ 2 ];
		transformed += morphTarget3 * morphTargetInfluences[ 3 ];
		#ifndef USE_MORPHNORMALS
			transformed += morphTarget4 * morphTargetInfluences[ 4 ];
			transformed += morphTarget5 * morphTargetInfluences[ 5 ];
			transformed += morphTarget6 * morphTargetInfluences[ 6 ];
			transformed += morphTarget7 * morphTargetInfluences[ 7 ];
		#endif
	#endif
#endif`,normal_fragment_begin:`float faceDirection = gl_FrontFacing ? 1.0 : - 1.0;
#ifdef FLAT_SHADED
	vec3 fdx = dFdx( vViewPosition );
	vec3 fdy = dFdy( vViewPosition );
	vec3 normal = normalize( cross( fdx, fdy ) );
#else
	vec3 normal = normalize( vNormal );
	#ifdef DOUBLE_SIDED
		normal *= faceDirection;
	#endif
#endif
#if defined( USE_NORMALMAP_TANGENTSPACE ) || defined( USE_CLEARCOAT_NORMALMAP ) || defined( USE_ANISOTROPY )
	#ifdef USE_TANGENT
		mat3 tbn = mat3( normalize( vTangent ), normalize( vBitangent ), normal );
	#else
		mat3 tbn = getTangentFrame( - vViewPosition, normal,
		#if defined( USE_NORMALMAP )
			vNormalMapUv
		#elif defined( USE_CLEARCOAT_NORMALMAP )
			vClearcoatNormalMapUv
		#else
			vUv
		#endif
		);
	#endif
	#if defined( DOUBLE_SIDED ) && ! defined( FLAT_SHADED )
		tbn[0] *= faceDirection;
		tbn[1] *= faceDirection;
	#endif
#endif
#ifdef USE_CLEARCOAT_NORMALMAP
	#ifdef USE_TANGENT
		mat3 tbn2 = mat3( normalize( vTangent ), normalize( vBitangent ), normal );
	#else
		mat3 tbn2 = getTangentFrame( - vViewPosition, normal, vClearcoatNormalMapUv );
	#endif
	#if defined( DOUBLE_SIDED ) && ! defined( FLAT_SHADED )
		tbn2[0] *= faceDirection;
		tbn2[1] *= faceDirection;
	#endif
#endif
vec3 nonPerturbedNormal = normal;`,normal_fragment_maps:`#ifdef USE_NORMALMAP_OBJECTSPACE
	normal = texture2D( normalMap, vNormalMapUv ).xyz * 2.0 - 1.0;
	#ifdef FLIP_SIDED
		normal = - normal;
	#endif
	#ifdef DOUBLE_SIDED
		normal = normal * faceDirection;
	#endif
	normal = normalize( normalMatrix * normal );
#elif defined( USE_NORMALMAP_TANGENTSPACE )
	vec3 mapN = texture2D( normalMap, vNormalMapUv ).xyz * 2.0 - 1.0;
	mapN.xy *= normalScale;
	normal = normalize( tbn * mapN );
#elif defined( USE_BUMPMAP )
	normal = perturbNormalArb( - vViewPosition, normal, dHdxy_fwd(), faceDirection );
#endif`,normal_pars_fragment:`#ifndef FLAT_SHADED
	varying vec3 vNormal;
	#ifdef USE_TANGENT
		varying vec3 vTangent;
		varying vec3 vBitangent;
	#endif
#endif`,normal_pars_vertex:`#ifndef FLAT_SHADED
	varying vec3 vNormal;
	#ifdef USE_TANGENT
		varying vec3 vTangent;
		varying vec3 vBitangent;
	#endif
#endif`,normal_vertex:`#ifndef FLAT_SHADED
	vNormal = normalize( transformedNormal );
	#ifdef USE_TANGENT
		vTangent = normalize( transformedTangent );
		vBitangent = normalize( cross( vNormal, vTangent ) * tangent.w );
	#endif
#endif`,normalmap_pars_fragment:`#ifdef USE_NORMALMAP
	uniform sampler2D normalMap;
	uniform vec2 normalScale;
#endif
#ifdef USE_NORMALMAP_OBJECTSPACE
	uniform mat3 normalMatrix;
#endif
#if ! defined ( USE_TANGENT ) && ( defined ( USE_NORMALMAP_TANGENTSPACE ) || defined ( USE_CLEARCOAT_NORMALMAP ) || defined( USE_ANISOTROPY ) )
	mat3 getTangentFrame( vec3 eye_pos, vec3 surf_norm, vec2 uv ) {
		vec3 q0 = dFdx( eye_pos.xyz );
		vec3 q1 = dFdy( eye_pos.xyz );
		vec2 st0 = dFdx( uv.st );
		vec2 st1 = dFdy( uv.st );
		vec3 N = surf_norm;
		vec3 q1perp = cross( q1, N );
		vec3 q0perp = cross( N, q0 );
		vec3 T = q1perp * st0.x + q0perp * st1.x;
		vec3 B = q1perp * st0.y + q0perp * st1.y;
		float det = max( dot( T, T ), dot( B, B ) );
		float scale = ( det == 0.0 ) ? 0.0 : inversesqrt( det );
		return mat3( T * scale, B * scale, N );
	}
#endif`,clearcoat_normal_fragment_begin:`#ifdef USE_CLEARCOAT
	vec3 clearcoatNormal = nonPerturbedNormal;
#endif`,clearcoat_normal_fragment_maps:`#ifdef USE_CLEARCOAT_NORMALMAP
	vec3 clearcoatMapN = texture2D( clearcoatNormalMap, vClearcoatNormalMapUv ).xyz * 2.0 - 1.0;
	clearcoatMapN.xy *= clearcoatNormalScale;
	clearcoatNormal = normalize( tbn2 * clearcoatMapN );
#endif`,clearcoat_pars_fragment:`#ifdef USE_CLEARCOATMAP
	uniform sampler2D clearcoatMap;
#endif
#ifdef USE_CLEARCOAT_NORMALMAP
	uniform sampler2D clearcoatNormalMap;
	uniform vec2 clearcoatNormalScale;
#endif
#ifdef USE_CLEARCOAT_ROUGHNESSMAP
	uniform sampler2D clearcoatRoughnessMap;
#endif`,iridescence_pars_fragment:`#ifdef USE_IRIDESCENCEMAP
	uniform sampler2D iridescenceMap;
#endif
#ifdef USE_IRIDESCENCE_THICKNESSMAP
	uniform sampler2D iridescenceThicknessMap;
#endif`,opaque_fragment:`#ifdef OPAQUE
diffuseColor.a = 1.0;
#endif
#ifdef USE_TRANSMISSION
diffuseColor.a *= material.transmissionAlpha;
#endif
gl_FragColor = vec4( outgoingLight, diffuseColor.a );`,packing:`vec3 packNormalToRGB( const in vec3 normal ) {
	return normalize( normal ) * 0.5 + 0.5;
}
vec3 unpackRGBToNormal( const in vec3 rgb ) {
	return 2.0 * rgb.xyz - 1.0;
}
const float PackUpscale = 256. / 255.;const float UnpackDownscale = 255. / 256.;
const vec3 PackFactors = vec3( 256. * 256. * 256., 256. * 256., 256. );
const vec4 UnpackFactors = UnpackDownscale / vec4( PackFactors, 1. );
const float ShiftRight8 = 1. / 256.;
vec4 packDepthToRGBA( const in float v ) {
	vec4 r = vec4( fract( v * PackFactors ), v );
	r.yzw -= r.xyz * ShiftRight8;	return r * PackUpscale;
}
float unpackRGBAToDepth( const in vec4 v ) {
	return dot( v, UnpackFactors );
}
vec2 packDepthToRG( in highp float v ) {
	return packDepthToRGBA( v ).yx;
}
float unpackRGToDepth( const in highp vec2 v ) {
	return unpackRGBAToDepth( vec4( v.xy, 0.0, 0.0 ) );
}
vec4 pack2HalfToRGBA( vec2 v ) {
	vec4 r = vec4( v.x, fract( v.x * 255.0 ), v.y, fract( v.y * 255.0 ) );
	return vec4( r.x - r.y / 255.0, r.y, r.z - r.w / 255.0, r.w );
}
vec2 unpackRGBATo2Half( vec4 v ) {
	return vec2( v.x + ( v.y / 255.0 ), v.z + ( v.w / 255.0 ) );
}
float viewZToOrthographicDepth( const in float viewZ, const in float near, const in float far ) {
	return ( viewZ + near ) / ( near - far );
}
float orthographicDepthToViewZ( const in float depth, const in float near, const in float far ) {
	return depth * ( near - far ) - near;
}
float viewZToPerspectiveDepth( const in float viewZ, const in float near, const in float far ) {
	return ( ( near + viewZ ) * far ) / ( ( far - near ) * viewZ );
}
float perspectiveDepthToViewZ( const in float depth, const in float near, const in float far ) {
	return ( near * far ) / ( ( far - near ) * depth - far );
}`,premultiplied_alpha_fragment:`#ifdef PREMULTIPLIED_ALPHA
	gl_FragColor.rgb *= gl_FragColor.a;
#endif`,project_vertex:`vec4 mvPosition = vec4( transformed, 1.0 );
#ifdef USE_BATCHING
	mvPosition = batchingMatrix * mvPosition;
#endif
#ifdef USE_INSTANCING
	mvPosition = instanceMatrix * mvPosition;
#endif
mvPosition = modelViewMatrix * mvPosition;
gl_Position = projectionMatrix * mvPosition;`,dithering_fragment:`#ifdef DITHERING
	gl_FragColor.rgb = dithering( gl_FragColor.rgb );
#endif`,dithering_pars_fragment:`#ifdef DITHERING
	vec3 dithering( vec3 color ) {
		float grid_position = rand( gl_FragCoord.xy );
		vec3 dither_shift_RGB = vec3( 0.25 / 255.0, -0.25 / 255.0, 0.25 / 255.0 );
		dither_shift_RGB = mix( 2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position );
		return color + dither_shift_RGB;
	}
#endif`,roughnessmap_fragment:`float roughnessFactor = roughness;
#ifdef USE_ROUGHNESSMAP
	vec4 texelRoughness = texture2D( roughnessMap, vRoughnessMapUv );
	roughnessFactor *= texelRoughness.g;
#endif`,roughnessmap_pars_fragment:`#ifdef USE_ROUGHNESSMAP
	uniform sampler2D roughnessMap;
#endif`,shadowmap_pars_fragment:`#if NUM_SPOT_LIGHT_COORDS > 0
	varying vec4 vSpotLightCoord[ NUM_SPOT_LIGHT_COORDS ];
#endif
#if NUM_SPOT_LIGHT_MAPS > 0
	uniform sampler2D spotLightMap[ NUM_SPOT_LIGHT_MAPS ];
#endif
#ifdef USE_SHADOWMAP
	#if NUM_DIR_LIGHT_SHADOWS > 0
		uniform sampler2D directionalShadowMap[ NUM_DIR_LIGHT_SHADOWS ];
		varying vec4 vDirectionalShadowCoord[ NUM_DIR_LIGHT_SHADOWS ];
		struct DirectionalLightShadow {
			float shadowBias;
			float shadowNormalBias;
			float shadowRadius;
			vec2 shadowMapSize;
		};
		uniform DirectionalLightShadow directionalLightShadows[ NUM_DIR_LIGHT_SHADOWS ];
	#endif
	#if NUM_SPOT_LIGHT_SHADOWS > 0
		uniform sampler2D spotShadowMap[ NUM_SPOT_LIGHT_SHADOWS ];
		struct SpotLightShadow {
			float shadowBias;
			float shadowNormalBias;
			float shadowRadius;
			vec2 shadowMapSize;
		};
		uniform SpotLightShadow spotLightShadows[ NUM_SPOT_LIGHT_SHADOWS ];
	#endif
	#if NUM_POINT_LIGHT_SHADOWS > 0
		uniform sampler2D pointShadowMap[ NUM_POINT_LIGHT_SHADOWS ];
		varying vec4 vPointShadowCoord[ NUM_POINT_LIGHT_SHADOWS ];
		struct PointLightShadow {
			float shadowBias;
			float shadowNormalBias;
			float shadowRadius;
			vec2 shadowMapSize;
			float shadowCameraNear;
			float shadowCameraFar;
		};
		uniform PointLightShadow pointLightShadows[ NUM_POINT_LIGHT_SHADOWS ];
	#endif
	float texture2DCompare( sampler2D depths, vec2 uv, float compare ) {
		return step( compare, unpackRGBAToDepth( texture2D( depths, uv ) ) );
	}
	vec2 texture2DDistribution( sampler2D shadow, vec2 uv ) {
		return unpackRGBATo2Half( texture2D( shadow, uv ) );
	}
	float VSMShadow (sampler2D shadow, vec2 uv, float compare ){
		float occlusion = 1.0;
		vec2 distribution = texture2DDistribution( shadow, uv );
		float hard_shadow = step( compare , distribution.x );
		if (hard_shadow != 1.0 ) {
			float distance = compare - distribution.x ;
			float variance = max( 0.00000, distribution.y * distribution.y );
			float softness_probability = variance / (variance + distance * distance );			softness_probability = clamp( ( softness_probability - 0.3 ) / ( 0.95 - 0.3 ), 0.0, 1.0 );			occlusion = clamp( max( hard_shadow, softness_probability ), 0.0, 1.0 );
		}
		return occlusion;
	}
	float getShadow( sampler2D shadowMap, vec2 shadowMapSize, float shadowBias, float shadowRadius, vec4 shadowCoord ) {
		float shadow = 1.0;
		shadowCoord.xyz /= shadowCoord.w;
		shadowCoord.z += shadowBias;
		bool inFrustum = shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0;
		bool frustumTest = inFrustum && shadowCoord.z <= 1.0;
		if ( frustumTest ) {
		#if defined( SHADOWMAP_TYPE_PCF )
			vec2 texelSize = vec2( 1.0 ) / shadowMapSize;
			float dx0 = - texelSize.x * shadowRadius;
			float dy0 = - texelSize.y * shadowRadius;
			float dx1 = + texelSize.x * shadowRadius;
			float dy1 = + texelSize.y * shadowRadius;
			float dx2 = dx0 / 2.0;
			float dy2 = dy0 / 2.0;
			float dx3 = dx1 / 2.0;
			float dy3 = dy1 / 2.0;
			shadow = (
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx0, dy0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( 0.0, dy0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx1, dy0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx2, dy2 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( 0.0, dy2 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx3, dy2 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx0, 0.0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx2, 0.0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy, shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx3, 0.0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx1, 0.0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx2, dy3 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( 0.0, dy3 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx3, dy3 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx0, dy1 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( 0.0, dy1 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, shadowCoord.xy + vec2( dx1, dy1 ), shadowCoord.z )
			) * ( 1.0 / 17.0 );
		#elif defined( SHADOWMAP_TYPE_PCF_SOFT )
			vec2 texelSize = vec2( 1.0 ) / shadowMapSize;
			float dx = texelSize.x;
			float dy = texelSize.y;
			vec2 uv = shadowCoord.xy;
			vec2 f = fract( uv * shadowMapSize + 0.5 );
			uv -= f * texelSize;
			shadow = (
				texture2DCompare( shadowMap, uv, shadowCoord.z ) +
				texture2DCompare( shadowMap, uv + vec2( dx, 0.0 ), shadowCoord.z ) +
				texture2DCompare( shadowMap, uv + vec2( 0.0, dy ), shadowCoord.z ) +
				texture2DCompare( shadowMap, uv + texelSize, shadowCoord.z ) +
				mix( texture2DCompare( shadowMap, uv + vec2( -dx, 0.0 ), shadowCoord.z ),
					 texture2DCompare( shadowMap, uv + vec2( 2.0 * dx, 0.0 ), shadowCoord.z ),
					 f.x ) +
				mix( texture2DCompare( shadowMap, uv + vec2( -dx, dy ), shadowCoord.z ),
					 texture2DCompare( shadowMap, uv + vec2( 2.0 * dx, dy ), shadowCoord.z ),
					 f.x ) +
				mix( texture2DCompare( shadowMap, uv + vec2( 0.0, -dy ), shadowCoord.z ),
					 texture2DCompare( shadowMap, uv + vec2( 0.0, 2.0 * dy ), shadowCoord.z ),
					 f.y ) +
				mix( texture2DCompare( shadowMap, uv + vec2( dx, -dy ), shadowCoord.z ),
					 texture2DCompare( shadowMap, uv + vec2( dx, 2.0 * dy ), shadowCoord.z ),
					 f.y ) +
				mix( mix( texture2DCompare( shadowMap, uv + vec2( -dx, -dy ), shadowCoord.z ),
						  texture2DCompare( shadowMap, uv + vec2( 2.0 * dx, -dy ), shadowCoord.z ),
						  f.x ),
					 mix( texture2DCompare( shadowMap, uv + vec2( -dx, 2.0 * dy ), shadowCoord.z ),
						  texture2DCompare( shadowMap, uv + vec2( 2.0 * dx, 2.0 * dy ), shadowCoord.z ),
						  f.x ),
					 f.y )
			) * ( 1.0 / 9.0 );
		#elif defined( SHADOWMAP_TYPE_VSM )
			shadow = VSMShadow( shadowMap, shadowCoord.xy, shadowCoord.z );
		#else
			shadow = texture2DCompare( shadowMap, shadowCoord.xy, shadowCoord.z );
		#endif
		}
		return shadow;
	}
	vec2 cubeToUV( vec3 v, float texelSizeY ) {
		vec3 absV = abs( v );
		float scaleToCube = 1.0 / max( absV.x, max( absV.y, absV.z ) );
		absV *= scaleToCube;
		v *= scaleToCube * ( 1.0 - 2.0 * texelSizeY );
		vec2 planar = v.xy;
		float almostATexel = 1.5 * texelSizeY;
		float almostOne = 1.0 - almostATexel;
		if ( absV.z >= almostOne ) {
			if ( v.z > 0.0 )
				planar.x = 4.0 - v.x;
		} else if ( absV.x >= almostOne ) {
			float signX = sign( v.x );
			planar.x = v.z * signX + 2.0 * signX;
		} else if ( absV.y >= almostOne ) {
			float signY = sign( v.y );
			planar.x = v.x + 2.0 * signY + 2.0;
			planar.y = v.z * signY - 2.0;
		}
		return vec2( 0.125, 0.25 ) * planar + vec2( 0.375, 0.75 );
	}
	float getPointShadow( sampler2D shadowMap, vec2 shadowMapSize, float shadowBias, float shadowRadius, vec4 shadowCoord, float shadowCameraNear, float shadowCameraFar ) {
		vec2 texelSize = vec2( 1.0 ) / ( shadowMapSize * vec2( 4.0, 2.0 ) );
		vec3 lightToPosition = shadowCoord.xyz;
		float dp = ( length( lightToPosition ) - shadowCameraNear ) / ( shadowCameraFar - shadowCameraNear );		dp += shadowBias;
		vec3 bd3D = normalize( lightToPosition );
		#if defined( SHADOWMAP_TYPE_PCF ) || defined( SHADOWMAP_TYPE_PCF_SOFT ) || defined( SHADOWMAP_TYPE_VSM )
			vec2 offset = vec2( - 1, 1 ) * shadowRadius * texelSize.y;
			return (
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.xyy, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.yyy, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.xyx, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.yyx, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.xxy, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.yxy, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.xxx, texelSize.y ), dp ) +
				texture2DCompare( shadowMap, cubeToUV( bd3D + offset.yxx, texelSize.y ), dp )
			) * ( 1.0 / 9.0 );
		#else
			return texture2DCompare( shadowMap, cubeToUV( bd3D, texelSize.y ), dp );
		#endif
	}
#endif`,shadowmap_pars_vertex:`#if NUM_SPOT_LIGHT_COORDS > 0
	uniform mat4 spotLightMatrix[ NUM_SPOT_LIGHT_COORDS ];
	varying vec4 vSpotLightCoord[ NUM_SPOT_LIGHT_COORDS ];
#endif
#ifdef USE_SHADOWMAP
	#if NUM_DIR_LIGHT_SHADOWS > 0
		uniform mat4 directionalShadowMatrix[ NUM_DIR_LIGHT_SHADOWS ];
		varying vec4 vDirectionalShadowCoord[ NUM_DIR_LIGHT_SHADOWS ];
		struct DirectionalLightShadow {
			float shadowBias;
			float shadowNormalBias;
			float shadowRadius;
			vec2 shadowMapSize;
		};
		uniform DirectionalLightShadow directionalLightShadows[ NUM_DIR_LIGHT_SHADOWS ];
	#endif
	#if NUM_SPOT_LIGHT_SHADOWS > 0
		struct SpotLightShadow {
			float shadowBias;
			float shadowNormalBias;
			float shadowRadius;
			vec2 shadowMapSize;
		};
		uniform SpotLightShadow spotLightShadows[ NUM_SPOT_LIGHT_SHADOWS ];
	#endif
	#if NUM_POINT_LIGHT_SHADOWS > 0
		uniform mat4 pointShadowMatrix[ NUM_POINT_LIGHT_SHADOWS ];
		varying vec4 vPointShadowCoord[ NUM_POINT_LIGHT_SHADOWS ];
		struct PointLightShadow {
			float shadowBias;
			float shadowNormalBias;
			float shadowRadius;
			vec2 shadowMapSize;
			float shadowCameraNear;
			float shadowCameraFar;
		};
		uniform PointLightShadow pointLightShadows[ NUM_POINT_LIGHT_SHADOWS ];
	#endif
#endif`,shadowmap_vertex:`#if ( defined( USE_SHADOWMAP ) && ( NUM_DIR_LIGHT_SHADOWS > 0 || NUM_POINT_LIGHT_SHADOWS > 0 ) ) || ( NUM_SPOT_LIGHT_COORDS > 0 )
	vec3 shadowWorldNormal = inverseTransformDirection( transformedNormal, viewMatrix );
	vec4 shadowWorldPosition;
#endif
#if defined( USE_SHADOWMAP )
	#if NUM_DIR_LIGHT_SHADOWS > 0
		#pragma unroll_loop_start
		for ( int i = 0; i < NUM_DIR_LIGHT_SHADOWS; i ++ ) {
			shadowWorldPosition = worldPosition + vec4( shadowWorldNormal * directionalLightShadows[ i ].shadowNormalBias, 0 );
			vDirectionalShadowCoord[ i ] = directionalShadowMatrix[ i ] * shadowWorldPosition;
		}
		#pragma unroll_loop_end
	#endif
	#if NUM_POINT_LIGHT_SHADOWS > 0
		#pragma unroll_loop_start
		for ( int i = 0; i < NUM_POINT_LIGHT_SHADOWS; i ++ ) {
			shadowWorldPosition = worldPosition + vec4( shadowWorldNormal * pointLightShadows[ i ].shadowNormalBias, 0 );
			vPointShadowCoord[ i ] = pointShadowMatrix[ i ] * shadowWorldPosition;
		}
		#pragma unroll_loop_end
	#endif
#endif
#if NUM_SPOT_LIGHT_COORDS > 0
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_SPOT_LIGHT_COORDS; i ++ ) {
		shadowWorldPosition = worldPosition;
		#if ( defined( USE_SHADOWMAP ) && UNROLLED_LOOP_INDEX < NUM_SPOT_LIGHT_SHADOWS )
			shadowWorldPosition.xyz += shadowWorldNormal * spotLightShadows[ i ].shadowNormalBias;
		#endif
		vSpotLightCoord[ i ] = spotLightMatrix[ i ] * shadowWorldPosition;
	}
	#pragma unroll_loop_end
#endif`,shadowmask_pars_fragment:`float getShadowMask() {
	float shadow = 1.0;
	#ifdef USE_SHADOWMAP
	#if NUM_DIR_LIGHT_SHADOWS > 0
	DirectionalLightShadow directionalLight;
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_DIR_LIGHT_SHADOWS; i ++ ) {
		directionalLight = directionalLightShadows[ i ];
		shadow *= receiveShadow ? getShadow( directionalShadowMap[ i ], directionalLight.shadowMapSize, directionalLight.shadowBias, directionalLight.shadowRadius, vDirectionalShadowCoord[ i ] ) : 1.0;
	}
	#pragma unroll_loop_end
	#endif
	#if NUM_SPOT_LIGHT_SHADOWS > 0
	SpotLightShadow spotLight;
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_SPOT_LIGHT_SHADOWS; i ++ ) {
		spotLight = spotLightShadows[ i ];
		shadow *= receiveShadow ? getShadow( spotShadowMap[ i ], spotLight.shadowMapSize, spotLight.shadowBias, spotLight.shadowRadius, vSpotLightCoord[ i ] ) : 1.0;
	}
	#pragma unroll_loop_end
	#endif
	#if NUM_POINT_LIGHT_SHADOWS > 0
	PointLightShadow pointLight;
	#pragma unroll_loop_start
	for ( int i = 0; i < NUM_POINT_LIGHT_SHADOWS; i ++ ) {
		pointLight = pointLightShadows[ i ];
		shadow *= receiveShadow ? getPointShadow( pointShadowMap[ i ], pointLight.shadowMapSize, pointLight.shadowBias, pointLight.shadowRadius, vPointShadowCoord[ i ], pointLight.shadowCameraNear, pointLight.shadowCameraFar ) : 1.0;
	}
	#pragma unroll_loop_end
	#endif
	#endif
	return shadow;
}`,skinbase_vertex:`#ifdef USE_SKINNING
	mat4 boneMatX = getBoneMatrix( skinIndex.x );
	mat4 boneMatY = getBoneMatrix( skinIndex.y );
	mat4 boneMatZ = getBoneMatrix( skinIndex.z );
	mat4 boneMatW = getBoneMatrix( skinIndex.w );
#endif`,skinning_pars_vertex:`#ifdef USE_SKINNING
	uniform mat4 bindMatrix;
	uniform mat4 bindMatrixInverse;
	uniform highp sampler2D boneTexture;
	mat4 getBoneMatrix( const in float i ) {
		int size = textureSize( boneTexture, 0 ).x;
		int j = int( i ) * 4;
		int x = j % size;
		int y = j / size;
		vec4 v1 = texelFetch( boneTexture, ivec2( x, y ), 0 );
		vec4 v2 = texelFetch( boneTexture, ivec2( x + 1, y ), 0 );
		vec4 v3 = texelFetch( boneTexture, ivec2( x + 2, y ), 0 );
		vec4 v4 = texelFetch( boneTexture, ivec2( x + 3, y ), 0 );
		return mat4( v1, v2, v3, v4 );
	}
#endif`,skinning_vertex:`#ifdef USE_SKINNING
	vec4 skinVertex = bindMatrix * vec4( transformed, 1.0 );
	vec4 skinned = vec4( 0.0 );
	skinned += boneMatX * skinVertex * skinWeight.x;
	skinned += boneMatY * skinVertex * skinWeight.y;
	skinned += boneMatZ * skinVertex * skinWeight.z;
	skinned += boneMatW * skinVertex * skinWeight.w;
	transformed = ( bindMatrixInverse * skinned ).xyz;
#endif`,skinnormal_vertex:`#ifdef USE_SKINNING
	mat4 skinMatrix = mat4( 0.0 );
	skinMatrix += skinWeight.x * boneMatX;
	skinMatrix += skinWeight.y * boneMatY;
	skinMatrix += skinWeight.z * boneMatZ;
	skinMatrix += skinWeight.w * boneMatW;
	skinMatrix = bindMatrixInverse * skinMatrix * bindMatrix;
	objectNormal = vec4( skinMatrix * vec4( objectNormal, 0.0 ) ).xyz;
	#ifdef USE_TANGENT
		objectTangent = vec4( skinMatrix * vec4( objectTangent, 0.0 ) ).xyz;
	#endif
#endif`,specularmap_fragment:`float specularStrength;
#ifdef USE_SPECULARMAP
	vec4 texelSpecular = texture2D( specularMap, vSpecularMapUv );
	specularStrength = texelSpecular.r;
#else
	specularStrength = 1.0;
#endif`,specularmap_pars_fragment:`#ifdef USE_SPECULARMAP
	uniform sampler2D specularMap;
#endif`,tonemapping_fragment:`#if defined( TONE_MAPPING )
	gl_FragColor.rgb = toneMapping( gl_FragColor.rgb );
#endif`,tonemapping_pars_fragment:`#ifndef saturate
#define saturate( a ) clamp( a, 0.0, 1.0 )
#endif
uniform float toneMappingExposure;
vec3 LinearToneMapping( vec3 color ) {
	return saturate( toneMappingExposure * color );
}
vec3 ReinhardToneMapping( vec3 color ) {
	color *= toneMappingExposure;
	return saturate( color / ( vec3( 1.0 ) + color ) );
}
vec3 OptimizedCineonToneMapping( vec3 color ) {
	color *= toneMappingExposure;
	color = max( vec3( 0.0 ), color - 0.004 );
	return pow( ( color * ( 6.2 * color + 0.5 ) ) / ( color * ( 6.2 * color + 1.7 ) + 0.06 ), vec3( 2.2 ) );
}
vec3 RRTAndODTFit( vec3 v ) {
	vec3 a = v * ( v + 0.0245786 ) - 0.000090537;
	vec3 b = v * ( 0.983729 * v + 0.4329510 ) + 0.238081;
	return a / b;
}
vec3 ACESFilmicToneMapping( vec3 color ) {
	const mat3 ACESInputMat = mat3(
		vec3( 0.59719, 0.07600, 0.02840 ),		vec3( 0.35458, 0.90834, 0.13383 ),
		vec3( 0.04823, 0.01566, 0.83777 )
	);
	const mat3 ACESOutputMat = mat3(
		vec3(  1.60475, -0.10208, -0.00327 ),		vec3( -0.53108,  1.10813, -0.07276 ),
		vec3( -0.07367, -0.00605,  1.07602 )
	);
	color *= toneMappingExposure / 0.6;
	color = ACESInputMat * color;
	color = RRTAndODTFit( color );
	color = ACESOutputMat * color;
	return saturate( color );
}
const mat3 LINEAR_REC2020_TO_LINEAR_SRGB = mat3(
	vec3( 1.6605, - 0.1246, - 0.0182 ),
	vec3( - 0.5876, 1.1329, - 0.1006 ),
	vec3( - 0.0728, - 0.0083, 1.1187 )
);
const mat3 LINEAR_SRGB_TO_LINEAR_REC2020 = mat3(
	vec3( 0.6274, 0.0691, 0.0164 ),
	vec3( 0.3293, 0.9195, 0.0880 ),
	vec3( 0.0433, 0.0113, 0.8956 )
);
vec3 agxDefaultContrastApprox( vec3 x ) {
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return + 15.5 * x4 * x2
		- 40.14 * x4 * x
		+ 31.96 * x4
		- 6.868 * x2 * x
		+ 0.4298 * x2
		+ 0.1191 * x
		- 0.00232;
}
vec3 AgXToneMapping( vec3 color ) {
	const mat3 AgXInsetMatrix = mat3(
		vec3( 0.856627153315983, 0.137318972929847, 0.11189821299995 ),
		vec3( 0.0951212405381588, 0.761241990602591, 0.0767994186031903 ),
		vec3( 0.0482516061458583, 0.101439036467562, 0.811302368396859 )
	);
	const mat3 AgXOutsetMatrix = mat3(
		vec3( 1.1271005818144368, - 0.1413297634984383, - 0.14132976349843826 ),
		vec3( - 0.11060664309660323, 1.157823702216272, - 0.11060664309660294 ),
		vec3( - 0.016493938717834573, - 0.016493938717834257, 1.2519364065950405 )
	);
	const float AgxMinEv = - 12.47393;	const float AgxMaxEv = 4.026069;
	color *= toneMappingExposure;
	color = LINEAR_SRGB_TO_LINEAR_REC2020 * color;
	color = AgXInsetMatrix * color;
	color = max( color, 1e-10 );	color = log2( color );
	color = ( color - AgxMinEv ) / ( AgxMaxEv - AgxMinEv );
	color = clamp( color, 0.0, 1.0 );
	color = agxDefaultContrastApprox( color );
	color = AgXOutsetMatrix * color;
	color = pow( max( vec3( 0.0 ), color ), vec3( 2.2 ) );
	color = LINEAR_REC2020_TO_LINEAR_SRGB * color;
	color = clamp( color, 0.0, 1.0 );
	return color;
}
vec3 NeutralToneMapping( vec3 color ) {
	float startCompression = 0.8 - 0.04;
	float desaturation = 0.15;
	color *= toneMappingExposure;
	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;
	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) return color;
	float d = 1. - startCompression;
	float newPeak = 1. - d * d / (peak + d - startCompression);
	color *= newPeak / peak;
	float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
	return mix(color, vec3(1, 1, 1), g);
}
vec3 CustomToneMapping( vec3 color ) { return color; }`,transmission_fragment:`#ifdef USE_TRANSMISSION
	material.transmission = transmission;
	material.transmissionAlpha = 1.0;
	material.thickness = thickness;
	material.attenuationDistance = attenuationDistance;
	material.attenuationColor = attenuationColor;
	#ifdef USE_TRANSMISSIONMAP
		material.transmission *= texture2D( transmissionMap, vTransmissionMapUv ).r;
	#endif
	#ifdef USE_THICKNESSMAP
		material.thickness *= texture2D( thicknessMap, vThicknessMapUv ).g;
	#endif
	vec3 pos = vWorldPosition;
	vec3 v = normalize( cameraPosition - pos );
	vec3 n = inverseTransformDirection( normal, viewMatrix );
	vec4 transmitted = getIBLVolumeRefraction(
		n, v, material.roughness, material.diffuseColor, material.specularColor, material.specularF90,
		pos, modelMatrix, viewMatrix, projectionMatrix, material.ior, material.thickness,
		material.attenuationColor, material.attenuationDistance );
	material.transmissionAlpha = mix( material.transmissionAlpha, transmitted.a, material.transmission );
	totalDiffuse = mix( totalDiffuse, transmitted.rgb, material.transmission );
#endif`,transmission_pars_fragment:`#ifdef USE_TRANSMISSION
	uniform float transmission;
	uniform float thickness;
	uniform float attenuationDistance;
	uniform vec3 attenuationColor;
	#ifdef USE_TRANSMISSIONMAP
		uniform sampler2D transmissionMap;
	#endif
	#ifdef USE_THICKNESSMAP
		uniform sampler2D thicknessMap;
	#endif
	uniform vec2 transmissionSamplerSize;
	uniform sampler2D transmissionSamplerMap;
	uniform mat4 modelMatrix;
	uniform mat4 projectionMatrix;
	varying vec3 vWorldPosition;
	float w0( float a ) {
		return ( 1.0 / 6.0 ) * ( a * ( a * ( - a + 3.0 ) - 3.0 ) + 1.0 );
	}
	float w1( float a ) {
		return ( 1.0 / 6.0 ) * ( a *  a * ( 3.0 * a - 6.0 ) + 4.0 );
	}
	float w2( float a ){
		return ( 1.0 / 6.0 ) * ( a * ( a * ( - 3.0 * a + 3.0 ) + 3.0 ) + 1.0 );
	}
	float w3( float a ) {
		return ( 1.0 / 6.0 ) * ( a * a * a );
	}
	float g0( float a ) {
		return w0( a ) + w1( a );
	}
	float g1( float a ) {
		return w2( a ) + w3( a );
	}
	float h0( float a ) {
		return - 1.0 + w1( a ) / ( w0( a ) + w1( a ) );
	}
	float h1( float a ) {
		return 1.0 + w3( a ) / ( w2( a ) + w3( a ) );
	}
	vec4 bicubic( sampler2D tex, vec2 uv, vec4 texelSize, float lod ) {
		uv = uv * texelSize.zw + 0.5;
		vec2 iuv = floor( uv );
		vec2 fuv = fract( uv );
		float g0x = g0( fuv.x );
		float g1x = g1( fuv.x );
		float h0x = h0( fuv.x );
		float h1x = h1( fuv.x );
		float h0y = h0( fuv.y );
		float h1y = h1( fuv.y );
		vec2 p0 = ( vec2( iuv.x + h0x, iuv.y + h0y ) - 0.5 ) * texelSize.xy;
		vec2 p1 = ( vec2( iuv.x + h1x, iuv.y + h0y ) - 0.5 ) * texelSize.xy;
		vec2 p2 = ( vec2( iuv.x + h0x, iuv.y + h1y ) - 0.5 ) * texelSize.xy;
		vec2 p3 = ( vec2( iuv.x + h1x, iuv.y + h1y ) - 0.5 ) * texelSize.xy;
		return g0( fuv.y ) * ( g0x * textureLod( tex, p0, lod ) + g1x * textureLod( tex, p1, lod ) ) +
			g1( fuv.y ) * ( g0x * textureLod( tex, p2, lod ) + g1x * textureLod( tex, p3, lod ) );
	}
	vec4 textureBicubic( sampler2D sampler, vec2 uv, float lod ) {
		vec2 fLodSize = vec2( textureSize( sampler, int( lod ) ) );
		vec2 cLodSize = vec2( textureSize( sampler, int( lod + 1.0 ) ) );
		vec2 fLodSizeInv = 1.0 / fLodSize;
		vec2 cLodSizeInv = 1.0 / cLodSize;
		vec4 fSample = bicubic( sampler, uv, vec4( fLodSizeInv, fLodSize ), floor( lod ) );
		vec4 cSample = bicubic( sampler, uv, vec4( cLodSizeInv, cLodSize ), ceil( lod ) );
		return mix( fSample, cSample, fract( lod ) );
	}
	vec3 getVolumeTransmissionRay( const in vec3 n, const in vec3 v, const in float thickness, const in float ior, const in mat4 modelMatrix ) {
		vec3 refractionVector = refract( - v, normalize( n ), 1.0 / ior );
		vec3 modelScale;
		modelScale.x = length( vec3( modelMatrix[ 0 ].xyz ) );
		modelScale.y = length( vec3( modelMatrix[ 1 ].xyz ) );
		modelScale.z = length( vec3( modelMatrix[ 2 ].xyz ) );
		return normalize( refractionVector ) * thickness * modelScale;
	}
	float applyIorToRoughness( const in float roughness, const in float ior ) {
		return roughness * clamp( ior * 2.0 - 2.0, 0.0, 1.0 );
	}
	vec4 getTransmissionSample( const in vec2 fragCoord, const in float roughness, const in float ior ) {
		float lod = log2( transmissionSamplerSize.x ) * applyIorToRoughness( roughness, ior );
		return textureBicubic( transmissionSamplerMap, fragCoord.xy, lod );
	}
	vec3 volumeAttenuation( const in float transmissionDistance, const in vec3 attenuationColor, const in float attenuationDistance ) {
		if ( isinf( attenuationDistance ) ) {
			return vec3( 1.0 );
		} else {
			vec3 attenuationCoefficient = -log( attenuationColor ) / attenuationDistance;
			vec3 transmittance = exp( - attenuationCoefficient * transmissionDistance );			return transmittance;
		}
	}
	vec4 getIBLVolumeRefraction( const in vec3 n, const in vec3 v, const in float roughness, const in vec3 diffuseColor,
		const in vec3 specularColor, const in float specularF90, const in vec3 position, const in mat4 modelMatrix,
		const in mat4 viewMatrix, const in mat4 projMatrix, const in float ior, const in float thickness,
		const in vec3 attenuationColor, const in float attenuationDistance ) {
		vec3 transmissionRay = getVolumeTransmissionRay( n, v, thickness, ior, modelMatrix );
		vec3 refractedRayExit = position + transmissionRay;
		vec4 ndcPos = projMatrix * viewMatrix * vec4( refractedRayExit, 1.0 );
		vec2 refractionCoords = ndcPos.xy / ndcPos.w;
		refractionCoords += 1.0;
		refractionCoords /= 2.0;
		vec4 transmittedLight = getTransmissionSample( refractionCoords, roughness, ior );
		vec3 transmittance = diffuseColor * volumeAttenuation( length( transmissionRay ), attenuationColor, attenuationDistance );
		vec3 attenuatedColor = transmittance * transmittedLight.rgb;
		vec3 F = EnvironmentBRDF( n, v, specularColor, specularF90, roughness );
		float transmittanceFactor = ( transmittance.r + transmittance.g + transmittance.b ) / 3.0;
		return vec4( ( 1.0 - F ) * attenuatedColor, 1.0 - ( 1.0 - transmittedLight.a ) * transmittanceFactor );
	}
#endif`,uv_pars_fragment:`#if defined( USE_UV ) || defined( USE_ANISOTROPY )
	varying vec2 vUv;
#endif
#ifdef USE_MAP
	varying vec2 vMapUv;
#endif
#ifdef USE_ALPHAMAP
	varying vec2 vAlphaMapUv;
#endif
#ifdef USE_LIGHTMAP
	varying vec2 vLightMapUv;
#endif
#ifdef USE_AOMAP
	varying vec2 vAoMapUv;
#endif
#ifdef USE_BUMPMAP
	varying vec2 vBumpMapUv;
#endif
#ifdef USE_NORMALMAP
	varying vec2 vNormalMapUv;
#endif
#ifdef USE_EMISSIVEMAP
	varying vec2 vEmissiveMapUv;
#endif
#ifdef USE_METALNESSMAP
	varying vec2 vMetalnessMapUv;
#endif
#ifdef USE_ROUGHNESSMAP
	varying vec2 vRoughnessMapUv;
#endif
#ifdef USE_ANISOTROPYMAP
	varying vec2 vAnisotropyMapUv;
#endif
#ifdef USE_CLEARCOATMAP
	varying vec2 vClearcoatMapUv;
#endif
#ifdef USE_CLEARCOAT_NORMALMAP
	varying vec2 vClearcoatNormalMapUv;
#endif
#ifdef USE_CLEARCOAT_ROUGHNESSMAP
	varying vec2 vClearcoatRoughnessMapUv;
#endif
#ifdef USE_IRIDESCENCEMAP
	varying vec2 vIridescenceMapUv;
#endif
#ifdef USE_IRIDESCENCE_THICKNESSMAP
	varying vec2 vIridescenceThicknessMapUv;
#endif
#ifdef USE_SHEEN_COLORMAP
	varying vec2 vSheenColorMapUv;
#endif
#ifdef USE_SHEEN_ROUGHNESSMAP
	varying vec2 vSheenRoughnessMapUv;
#endif
#ifdef USE_SPECULARMAP
	varying vec2 vSpecularMapUv;
#endif
#ifdef USE_SPECULAR_COLORMAP
	varying vec2 vSpecularColorMapUv;
#endif
#ifdef USE_SPECULAR_INTENSITYMAP
	varying vec2 vSpecularIntensityMapUv;
#endif
#ifdef USE_TRANSMISSIONMAP
	uniform mat3 transmissionMapTransform;
	varying vec2 vTransmissionMapUv;
#endif
#ifdef USE_THICKNESSMAP
	uniform mat3 thicknessMapTransform;
	varying vec2 vThicknessMapUv;
#endif`,uv_pars_vertex:`#if defined( USE_UV ) || defined( USE_ANISOTROPY )
	varying vec2 vUv;
#endif
#ifdef USE_MAP
	uniform mat3 mapTransform;
	varying vec2 vMapUv;
#endif
#ifdef USE_ALPHAMAP
	uniform mat3 alphaMapTransform;
	varying vec2 vAlphaMapUv;
#endif
#ifdef USE_LIGHTMAP
	uniform mat3 lightMapTransform;
	varying vec2 vLightMapUv;
#endif
#ifdef USE_AOMAP
	uniform mat3 aoMapTransform;
	varying vec2 vAoMapUv;
#endif
#ifdef USE_BUMPMAP
	uniform mat3 bumpMapTransform;
	varying vec2 vBumpMapUv;
#endif
#ifdef USE_NORMALMAP
	uniform mat3 normalMapTransform;
	varying vec2 vNormalMapUv;
#endif
#ifdef USE_DISPLACEMENTMAP
	uniform mat3 displacementMapTransform;
	varying vec2 vDisplacementMapUv;
#endif
#ifdef USE_EMISSIVEMAP
	uniform mat3 emissiveMapTransform;
	varying vec2 vEmissiveMapUv;
#endif
#ifdef USE_METALNESSMAP
	uniform mat3 metalnessMapTransform;
	varying vec2 vMetalnessMapUv;
#endif
#ifdef USE_ROUGHNESSMAP
	uniform mat3 roughnessMapTransform;
	varying vec2 vRoughnessMapUv;
#endif
#ifdef USE_ANISOTROPYMAP
	uniform mat3 anisotropyMapTransform;
	varying vec2 vAnisotropyMapUv;
#endif
#ifdef USE_CLEARCOATMAP
	uniform mat3 clearcoatMapTransform;
	varying vec2 vClearcoatMapUv;
#endif
#ifdef USE_CLEARCOAT_NORMALMAP
	uniform mat3 clearcoatNormalMapTransform;
	varying vec2 vClearcoatNormalMapUv;
#endif
#ifdef USE_CLEARCOAT_ROUGHNESSMAP
	uniform mat3 clearcoatRoughnessMapTransform;
	varying vec2 vClearcoatRoughnessMapUv;
#endif
#ifdef USE_SHEEN_COLORMAP
	uniform mat3 sheenColorMapTransform;
	varying vec2 vSheenColorMapUv;
#endif
#ifdef USE_SHEEN_ROUGHNESSMAP
	uniform mat3 sheenRoughnessMapTransform;
	varying vec2 vSheenRoughnessMapUv;
#endif
#ifdef USE_IRIDESCENCEMAP
	uniform mat3 iridescenceMapTransform;
	varying vec2 vIridescenceMapUv;
#endif
#ifdef USE_IRIDESCENCE_THICKNESSMAP
	uniform mat3 iridescenceThicknessMapTransform;
	varying vec2 vIridescenceThicknessMapUv;
#endif
#ifdef USE_SPECULARMAP
	uniform mat3 specularMapTransform;
	varying vec2 vSpecularMapUv;
#endif
#ifdef USE_SPECULAR_COLORMAP
	uniform mat3 specularColorMapTransform;
	varying vec2 vSpecularColorMapUv;
#endif
#ifdef USE_SPECULAR_INTENSITYMAP
	uniform mat3 specularIntensityMapTransform;
	varying vec2 vSpecularIntensityMapUv;
#endif
#ifdef USE_TRANSMISSIONMAP
	uniform mat3 transmissionMapTransform;
	varying vec2 vTransmissionMapUv;
#endif
#ifdef USE_THICKNESSMAP
	uniform mat3 thicknessMapTransform;
	varying vec2 vThicknessMapUv;
#endif`,uv_vertex:`#if defined( USE_UV ) || defined( USE_ANISOTROPY )
	vUv = vec3( uv, 1 ).xy;
#endif
#ifdef USE_MAP
	vMapUv = ( mapTransform * vec3( MAP_UV, 1 ) ).xy;
#endif
#ifdef USE_ALPHAMAP
	vAlphaMapUv = ( alphaMapTransform * vec3( ALPHAMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_LIGHTMAP
	vLightMapUv = ( lightMapTransform * vec3( LIGHTMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_AOMAP
	vAoMapUv = ( aoMapTransform * vec3( AOMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_BUMPMAP
	vBumpMapUv = ( bumpMapTransform * vec3( BUMPMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_NORMALMAP
	vNormalMapUv = ( normalMapTransform * vec3( NORMALMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_DISPLACEMENTMAP
	vDisplacementMapUv = ( displacementMapTransform * vec3( DISPLACEMENTMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_EMISSIVEMAP
	vEmissiveMapUv = ( emissiveMapTransform * vec3( EMISSIVEMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_METALNESSMAP
	vMetalnessMapUv = ( metalnessMapTransform * vec3( METALNESSMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_ROUGHNESSMAP
	vRoughnessMapUv = ( roughnessMapTransform * vec3( ROUGHNESSMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_ANISOTROPYMAP
	vAnisotropyMapUv = ( anisotropyMapTransform * vec3( ANISOTROPYMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_CLEARCOATMAP
	vClearcoatMapUv = ( clearcoatMapTransform * vec3( CLEARCOATMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_CLEARCOAT_NORMALMAP
	vClearcoatNormalMapUv = ( clearcoatNormalMapTransform * vec3( CLEARCOAT_NORMALMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_CLEARCOAT_ROUGHNESSMAP
	vClearcoatRoughnessMapUv = ( clearcoatRoughnessMapTransform * vec3( CLEARCOAT_ROUGHNESSMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_IRIDESCENCEMAP
	vIridescenceMapUv = ( iridescenceMapTransform * vec3( IRIDESCENCEMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_IRIDESCENCE_THICKNESSMAP
	vIridescenceThicknessMapUv = ( iridescenceThicknessMapTransform * vec3( IRIDESCENCE_THICKNESSMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_SHEEN_COLORMAP
	vSheenColorMapUv = ( sheenColorMapTransform * vec3( SHEEN_COLORMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_SHEEN_ROUGHNESSMAP
	vSheenRoughnessMapUv = ( sheenRoughnessMapTransform * vec3( SHEEN_ROUGHNESSMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_SPECULARMAP
	vSpecularMapUv = ( specularMapTransform * vec3( SPECULARMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_SPECULAR_COLORMAP
	vSpecularColorMapUv = ( specularColorMapTransform * vec3( SPECULAR_COLORMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_SPECULAR_INTENSITYMAP
	vSpecularIntensityMapUv = ( specularIntensityMapTransform * vec3( SPECULAR_INTENSITYMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_TRANSMISSIONMAP
	vTransmissionMapUv = ( transmissionMapTransform * vec3( TRANSMISSIONMAP_UV, 1 ) ).xy;
#endif
#ifdef USE_THICKNESSMAP
	vThicknessMapUv = ( thicknessMapTransform * vec3( THICKNESSMAP_UV, 1 ) ).xy;
#endif`,worldpos_vertex:`#if defined( USE_ENVMAP ) || defined( DISTANCE ) || defined ( USE_SHADOWMAP ) || defined ( USE_TRANSMISSION ) || NUM_SPOT_LIGHT_COORDS > 0
	vec4 worldPosition = vec4( transformed, 1.0 );
	#ifdef USE_BATCHING
		worldPosition = batchingMatrix * worldPosition;
	#endif
	#ifdef USE_INSTANCING
		worldPosition = instanceMatrix * worldPosition;
	#endif
	worldPosition = modelMatrix * worldPosition;
#endif`,background_vert:`varying vec2 vUv;
uniform mat3 uvTransform;
void main() {
	vUv = ( uvTransform * vec3( uv, 1 ) ).xy;
	gl_Position = vec4( position.xy, 1.0, 1.0 );
}`,background_frag:`uniform sampler2D t2D;
uniform float backgroundIntensity;
varying vec2 vUv;
void main() {
	vec4 texColor = texture2D( t2D, vUv );
	#ifdef DECODE_VIDEO_TEXTURE
		texColor = vec4( mix( pow( texColor.rgb * 0.9478672986 + vec3( 0.0521327014 ), vec3( 2.4 ) ), texColor.rgb * 0.0773993808, vec3( lessThanEqual( texColor.rgb, vec3( 0.04045 ) ) ) ), texColor.w );
	#endif
	texColor.rgb *= backgroundIntensity;
	gl_FragColor = texColor;
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
}`,backgroundCube_vert:`varying vec3 vWorldDirection;
#include <common>
void main() {
	vWorldDirection = transformDirection( position, modelMatrix );
	#include <begin_vertex>
	#include <project_vertex>
	gl_Position.z = gl_Position.w;
}`,backgroundCube_frag:`#ifdef ENVMAP_TYPE_CUBE
	uniform samplerCube envMap;
#elif defined( ENVMAP_TYPE_CUBE_UV )
	uniform sampler2D envMap;
#endif
uniform float flipEnvMap;
uniform float backgroundBlurriness;
uniform float backgroundIntensity;
uniform mat3 backgroundRotation;
varying vec3 vWorldDirection;
#include <cube_uv_reflection_fragment>
void main() {
	#ifdef ENVMAP_TYPE_CUBE
		vec4 texColor = textureCube( envMap, backgroundRotation * vec3( flipEnvMap * vWorldDirection.x, vWorldDirection.yz ) );
	#elif defined( ENVMAP_TYPE_CUBE_UV )
		vec4 texColor = textureCubeUV( envMap, backgroundRotation * vWorldDirection, backgroundBlurriness );
	#else
		vec4 texColor = vec4( 0.0, 0.0, 0.0, 1.0 );
	#endif
	texColor.rgb *= backgroundIntensity;
	gl_FragColor = texColor;
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
}`,cube_vert:`varying vec3 vWorldDirection;
#include <common>
void main() {
	vWorldDirection = transformDirection( position, modelMatrix );
	#include <begin_vertex>
	#include <project_vertex>
	gl_Position.z = gl_Position.w;
}`,cube_frag:`uniform samplerCube tCube;
uniform float tFlip;
uniform float opacity;
varying vec3 vWorldDirection;
void main() {
	vec4 texColor = textureCube( tCube, vec3( tFlip * vWorldDirection.x, vWorldDirection.yz ) );
	gl_FragColor = texColor;
	gl_FragColor.a *= opacity;
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
}`,depth_vert:`#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
varying vec2 vHighPrecisionZW;
void main() {
	#include <uv_vertex>
	#include <batching_vertex>
	#include <skinbase_vertex>
	#include <morphinstance_vertex>
	#ifdef USE_DISPLACEMENTMAP
		#include <beginnormal_vertex>
		#include <morphnormal_vertex>
		#include <skinnormal_vertex>
	#endif
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	vHighPrecisionZW = gl_Position.zw;
}`,depth_frag:`#if DEPTH_PACKING == 3200
	uniform float opacity;
#endif
#include <common>
#include <packing>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
varying vec2 vHighPrecisionZW;
void main() {
	vec4 diffuseColor = vec4( 1.0 );
	#include <clipping_planes_fragment>
	#if DEPTH_PACKING == 3200
		diffuseColor.a = opacity;
	#endif
	#include <map_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <logdepthbuf_fragment>
	float fragCoordZ = 0.5 * vHighPrecisionZW[0] / vHighPrecisionZW[1] + 0.5;
	#if DEPTH_PACKING == 3200
		gl_FragColor = vec4( vec3( 1.0 - fragCoordZ ), opacity );
	#elif DEPTH_PACKING == 3201
		gl_FragColor = packDepthToRGBA( fragCoordZ );
	#endif
}`,distanceRGBA_vert:`#define DISTANCE
varying vec3 vWorldPosition;
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <batching_vertex>
	#include <skinbase_vertex>
	#include <morphinstance_vertex>
	#ifdef USE_DISPLACEMENTMAP
		#include <beginnormal_vertex>
		#include <morphnormal_vertex>
		#include <skinnormal_vertex>
	#endif
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <worldpos_vertex>
	#include <clipping_planes_vertex>
	vWorldPosition = worldPosition.xyz;
}`,distanceRGBA_frag:`#define DISTANCE
uniform vec3 referencePosition;
uniform float nearDistance;
uniform float farDistance;
varying vec3 vWorldPosition;
#include <common>
#include <packing>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <clipping_planes_pars_fragment>
void main () {
	vec4 diffuseColor = vec4( 1.0 );
	#include <clipping_planes_fragment>
	#include <map_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	float dist = length( vWorldPosition - referencePosition );
	dist = ( dist - nearDistance ) / ( farDistance - nearDistance );
	dist = saturate( dist );
	gl_FragColor = packDepthToRGBA( dist );
}`,equirect_vert:`varying vec3 vWorldDirection;
#include <common>
void main() {
	vWorldDirection = transformDirection( position, modelMatrix );
	#include <begin_vertex>
	#include <project_vertex>
}`,equirect_frag:`uniform sampler2D tEquirect;
varying vec3 vWorldDirection;
#include <common>
void main() {
	vec3 direction = normalize( vWorldDirection );
	vec2 sampleUV = equirectUv( direction );
	gl_FragColor = texture2D( tEquirect, sampleUV );
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
}`,linedashed_vert:`uniform float scale;
attribute float lineDistance;
varying float vLineDistance;
#include <common>
#include <uv_pars_vertex>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <morphtarget_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	vLineDistance = scale * lineDistance;
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	#include <fog_vertex>
}`,linedashed_frag:`uniform vec3 diffuse;
uniform float opacity;
uniform float dashSize;
uniform float totalSize;
varying float vLineDistance;
#include <common>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <fog_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	if ( mod( vLineDistance, totalSize ) > dashSize ) {
		discard;
	}
	vec3 outgoingLight = vec3( 0.0 );
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	outgoingLight = diffuseColor.rgb;
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
}`,meshbasic_vert:`#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <envmap_pars_vertex>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <batching_vertex>
	#if defined ( USE_ENVMAP ) || defined ( USE_SKINNING )
		#include <beginnormal_vertex>
		#include <morphnormal_vertex>
		#include <skinbase_vertex>
		#include <skinnormal_vertex>
		#include <defaultnormal_vertex>
	#endif
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	#include <worldpos_vertex>
	#include <envmap_vertex>
	#include <fog_vertex>
}`,meshbasic_frag:`uniform vec3 diffuse;
uniform float opacity;
#ifndef FLAT_SHADED
	varying vec3 vNormal;
#endif
#include <common>
#include <dithering_pars_fragment>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <aomap_pars_fragment>
#include <lightmap_pars_fragment>
#include <envmap_common_pars_fragment>
#include <envmap_pars_fragment>
#include <fog_pars_fragment>
#include <specularmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <specularmap_fragment>
	ReflectedLight reflectedLight = ReflectedLight( vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ) );
	#ifdef USE_LIGHTMAP
		vec4 lightMapTexel = texture2D( lightMap, vLightMapUv );
		reflectedLight.indirectDiffuse += lightMapTexel.rgb * lightMapIntensity * RECIPROCAL_PI;
	#else
		reflectedLight.indirectDiffuse += vec3( 1.0 );
	#endif
	#include <aomap_fragment>
	reflectedLight.indirectDiffuse *= diffuseColor.rgb;
	vec3 outgoingLight = reflectedLight.indirectDiffuse;
	#include <envmap_fragment>
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
	#include <dithering_fragment>
}`,meshlambert_vert:`#define LAMBERT
varying vec3 vViewPosition;
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <envmap_pars_vertex>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <normal_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <shadowmap_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <normal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	vViewPosition = - mvPosition.xyz;
	#include <worldpos_vertex>
	#include <envmap_vertex>
	#include <shadowmap_vertex>
	#include <fog_vertex>
}`,meshlambert_frag:`#define LAMBERT
uniform vec3 diffuse;
uniform vec3 emissive;
uniform float opacity;
#include <common>
#include <packing>
#include <dithering_pars_fragment>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <aomap_pars_fragment>
#include <lightmap_pars_fragment>
#include <emissivemap_pars_fragment>
#include <envmap_common_pars_fragment>
#include <envmap_pars_fragment>
#include <fog_pars_fragment>
#include <bsdfs>
#include <lights_pars_begin>
#include <normal_pars_fragment>
#include <lights_lambert_pars_fragment>
#include <shadowmap_pars_fragment>
#include <bumpmap_pars_fragment>
#include <normalmap_pars_fragment>
#include <specularmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	ReflectedLight reflectedLight = ReflectedLight( vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ) );
	vec3 totalEmissiveRadiance = emissive;
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <specularmap_fragment>
	#include <normal_fragment_begin>
	#include <normal_fragment_maps>
	#include <emissivemap_fragment>
	#include <lights_lambert_fragment>
	#include <lights_fragment_begin>
	#include <lights_fragment_maps>
	#include <lights_fragment_end>
	#include <aomap_fragment>
	vec3 outgoingLight = reflectedLight.directDiffuse + reflectedLight.indirectDiffuse + totalEmissiveRadiance;
	#include <envmap_fragment>
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
	#include <dithering_fragment>
}`,meshmatcap_vert:`#define MATCAP
varying vec3 vViewPosition;
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <color_pars_vertex>
#include <displacementmap_pars_vertex>
#include <fog_pars_vertex>
#include <normal_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <normal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	#include <fog_vertex>
	vViewPosition = - mvPosition.xyz;
}`,meshmatcap_frag:`#define MATCAP
uniform vec3 diffuse;
uniform float opacity;
uniform sampler2D matcap;
varying vec3 vViewPosition;
#include <common>
#include <dithering_pars_fragment>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <fog_pars_fragment>
#include <normal_pars_fragment>
#include <bumpmap_pars_fragment>
#include <normalmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <normal_fragment_begin>
	#include <normal_fragment_maps>
	vec3 viewDir = normalize( vViewPosition );
	vec3 x = normalize( vec3( viewDir.z, 0.0, - viewDir.x ) );
	vec3 y = cross( viewDir, x );
	vec2 uv = vec2( dot( x, normal ), dot( y, normal ) ) * 0.495 + 0.5;
	#ifdef USE_MATCAP
		vec4 matcapColor = texture2D( matcap, uv );
	#else
		vec4 matcapColor = vec4( vec3( mix( 0.2, 0.8, uv.y ) ), 1.0 );
	#endif
	vec3 outgoingLight = diffuseColor.rgb * matcapColor.rgb;
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
	#include <dithering_fragment>
}`,meshnormal_vert:`#define NORMAL
#if defined( FLAT_SHADED ) || defined( USE_BUMPMAP ) || defined( USE_NORMALMAP_TANGENTSPACE )
	varying vec3 vViewPosition;
#endif
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <normal_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphinstance_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <normal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
#if defined( FLAT_SHADED ) || defined( USE_BUMPMAP ) || defined( USE_NORMALMAP_TANGENTSPACE )
	vViewPosition = - mvPosition.xyz;
#endif
}`,meshnormal_frag:`#define NORMAL
uniform float opacity;
#if defined( FLAT_SHADED ) || defined( USE_BUMPMAP ) || defined( USE_NORMALMAP_TANGENTSPACE )
	varying vec3 vViewPosition;
#endif
#include <packing>
#include <uv_pars_fragment>
#include <normal_pars_fragment>
#include <bumpmap_pars_fragment>
#include <normalmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( 0.0, 0.0, 0.0, opacity );
	#include <clipping_planes_fragment>
	#include <logdepthbuf_fragment>
	#include <normal_fragment_begin>
	#include <normal_fragment_maps>
	gl_FragColor = vec4( packNormalToRGB( normal ), diffuseColor.a );
	#ifdef OPAQUE
		gl_FragColor.a = 1.0;
	#endif
}`,meshphong_vert:`#define PHONG
varying vec3 vViewPosition;
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <envmap_pars_vertex>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <normal_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <shadowmap_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphcolor_vertex>
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphinstance_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <normal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	vViewPosition = - mvPosition.xyz;
	#include <worldpos_vertex>
	#include <envmap_vertex>
	#include <shadowmap_vertex>
	#include <fog_vertex>
}`,meshphong_frag:`#define PHONG
uniform vec3 diffuse;
uniform vec3 emissive;
uniform vec3 specular;
uniform float shininess;
uniform float opacity;
#include <common>
#include <packing>
#include <dithering_pars_fragment>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <aomap_pars_fragment>
#include <lightmap_pars_fragment>
#include <emissivemap_pars_fragment>
#include <envmap_common_pars_fragment>
#include <envmap_pars_fragment>
#include <fog_pars_fragment>
#include <bsdfs>
#include <lights_pars_begin>
#include <normal_pars_fragment>
#include <lights_phong_pars_fragment>
#include <shadowmap_pars_fragment>
#include <bumpmap_pars_fragment>
#include <normalmap_pars_fragment>
#include <specularmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	ReflectedLight reflectedLight = ReflectedLight( vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ) );
	vec3 totalEmissiveRadiance = emissive;
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <specularmap_fragment>
	#include <normal_fragment_begin>
	#include <normal_fragment_maps>
	#include <emissivemap_fragment>
	#include <lights_phong_fragment>
	#include <lights_fragment_begin>
	#include <lights_fragment_maps>
	#include <lights_fragment_end>
	#include <aomap_fragment>
	vec3 outgoingLight = reflectedLight.directDiffuse + reflectedLight.indirectDiffuse + reflectedLight.directSpecular + reflectedLight.indirectSpecular + totalEmissiveRadiance;
	#include <envmap_fragment>
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
	#include <dithering_fragment>
}`,meshphysical_vert:`#define STANDARD
varying vec3 vViewPosition;
#ifdef USE_TRANSMISSION
	varying vec3 vWorldPosition;
#endif
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <normal_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <shadowmap_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <normal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	vViewPosition = - mvPosition.xyz;
	#include <worldpos_vertex>
	#include <shadowmap_vertex>
	#include <fog_vertex>
#ifdef USE_TRANSMISSION
	vWorldPosition = worldPosition.xyz;
#endif
}`,meshphysical_frag:`#define STANDARD
#ifdef PHYSICAL
	#define IOR
	#define USE_SPECULAR
#endif
uniform vec3 diffuse;
uniform vec3 emissive;
uniform float roughness;
uniform float metalness;
uniform float opacity;
#ifdef IOR
	uniform float ior;
#endif
#ifdef USE_SPECULAR
	uniform float specularIntensity;
	uniform vec3 specularColor;
	#ifdef USE_SPECULAR_COLORMAP
		uniform sampler2D specularColorMap;
	#endif
	#ifdef USE_SPECULAR_INTENSITYMAP
		uniform sampler2D specularIntensityMap;
	#endif
#endif
#ifdef USE_CLEARCOAT
	uniform float clearcoat;
	uniform float clearcoatRoughness;
#endif
#ifdef USE_IRIDESCENCE
	uniform float iridescence;
	uniform float iridescenceIOR;
	uniform float iridescenceThicknessMinimum;
	uniform float iridescenceThicknessMaximum;
#endif
#ifdef USE_SHEEN
	uniform vec3 sheenColor;
	uniform float sheenRoughness;
	#ifdef USE_SHEEN_COLORMAP
		uniform sampler2D sheenColorMap;
	#endif
	#ifdef USE_SHEEN_ROUGHNESSMAP
		uniform sampler2D sheenRoughnessMap;
	#endif
#endif
#ifdef USE_ANISOTROPY
	uniform vec2 anisotropyVector;
	#ifdef USE_ANISOTROPYMAP
		uniform sampler2D anisotropyMap;
	#endif
#endif
varying vec3 vViewPosition;
#include <common>
#include <packing>
#include <dithering_pars_fragment>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <aomap_pars_fragment>
#include <lightmap_pars_fragment>
#include <emissivemap_pars_fragment>
#include <iridescence_fragment>
#include <cube_uv_reflection_fragment>
#include <envmap_common_pars_fragment>
#include <envmap_physical_pars_fragment>
#include <fog_pars_fragment>
#include <lights_pars_begin>
#include <normal_pars_fragment>
#include <lights_physical_pars_fragment>
#include <transmission_pars_fragment>
#include <shadowmap_pars_fragment>
#include <bumpmap_pars_fragment>
#include <normalmap_pars_fragment>
#include <clearcoat_pars_fragment>
#include <iridescence_pars_fragment>
#include <roughnessmap_pars_fragment>
#include <metalnessmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	ReflectedLight reflectedLight = ReflectedLight( vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ) );
	vec3 totalEmissiveRadiance = emissive;
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <roughnessmap_fragment>
	#include <metalnessmap_fragment>
	#include <normal_fragment_begin>
	#include <normal_fragment_maps>
	#include <clearcoat_normal_fragment_begin>
	#include <clearcoat_normal_fragment_maps>
	#include <emissivemap_fragment>
	#include <lights_physical_fragment>
	#include <lights_fragment_begin>
	#include <lights_fragment_maps>
	#include <lights_fragment_end>
	#include <aomap_fragment>
	vec3 totalDiffuse = reflectedLight.directDiffuse + reflectedLight.indirectDiffuse;
	vec3 totalSpecular = reflectedLight.directSpecular + reflectedLight.indirectSpecular;
	#include <transmission_fragment>
	vec3 outgoingLight = totalDiffuse + totalSpecular + totalEmissiveRadiance;
	#ifdef USE_SHEEN
		float sheenEnergyComp = 1.0 - 0.157 * max3( material.sheenColor );
		outgoingLight = outgoingLight * sheenEnergyComp + sheenSpecularDirect + sheenSpecularIndirect;
	#endif
	#ifdef USE_CLEARCOAT
		float dotNVcc = saturate( dot( geometryClearcoatNormal, geometryViewDir ) );
		vec3 Fcc = F_Schlick( material.clearcoatF0, material.clearcoatF90, dotNVcc );
		outgoingLight = outgoingLight * ( 1.0 - material.clearcoat * Fcc ) + ( clearcoatSpecularDirect + clearcoatSpecularIndirect ) * material.clearcoat;
	#endif
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
	#include <dithering_fragment>
}`,meshtoon_vert:`#define TOON
varying vec3 vViewPosition;
#include <common>
#include <batching_pars_vertex>
#include <uv_pars_vertex>
#include <displacementmap_pars_vertex>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <normal_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <shadowmap_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <normal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <displacementmap_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	vViewPosition = - mvPosition.xyz;
	#include <worldpos_vertex>
	#include <shadowmap_vertex>
	#include <fog_vertex>
}`,meshtoon_frag:`#define TOON
uniform vec3 diffuse;
uniform vec3 emissive;
uniform float opacity;
#include <common>
#include <packing>
#include <dithering_pars_fragment>
#include <color_pars_fragment>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <aomap_pars_fragment>
#include <lightmap_pars_fragment>
#include <emissivemap_pars_fragment>
#include <gradientmap_pars_fragment>
#include <fog_pars_fragment>
#include <bsdfs>
#include <lights_pars_begin>
#include <normal_pars_fragment>
#include <lights_toon_pars_fragment>
#include <shadowmap_pars_fragment>
#include <bumpmap_pars_fragment>
#include <normalmap_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	ReflectedLight reflectedLight = ReflectedLight( vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ), vec3( 0.0 ) );
	vec3 totalEmissiveRadiance = emissive;
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <color_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	#include <normal_fragment_begin>
	#include <normal_fragment_maps>
	#include <emissivemap_fragment>
	#include <lights_toon_fragment>
	#include <lights_fragment_begin>
	#include <lights_fragment_maps>
	#include <lights_fragment_end>
	#include <aomap_fragment>
	vec3 outgoingLight = reflectedLight.directDiffuse + reflectedLight.indirectDiffuse + totalEmissiveRadiance;
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
	#include <dithering_fragment>
}`,points_vert:`uniform float size;
uniform float scale;
#include <common>
#include <color_pars_vertex>
#include <fog_pars_vertex>
#include <morphtarget_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
#ifdef USE_POINTS_UV
	varying vec2 vUv;
	uniform mat3 uvTransform;
#endif
void main() {
	#ifdef USE_POINTS_UV
		vUv = ( uvTransform * vec3( uv, 1 ) ).xy;
	#endif
	#include <color_vertex>
	#include <morphinstance_vertex>
	#include <morphcolor_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <project_vertex>
	gl_PointSize = size;
	#ifdef USE_SIZEATTENUATION
		bool isPerspective = isPerspectiveMatrix( projectionMatrix );
		if ( isPerspective ) gl_PointSize *= ( scale / - mvPosition.z );
	#endif
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	#include <worldpos_vertex>
	#include <fog_vertex>
}`,points_frag:`uniform vec3 diffuse;
uniform float opacity;
#include <common>
#include <color_pars_fragment>
#include <map_particle_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <fog_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	vec3 outgoingLight = vec3( 0.0 );
	#include <logdepthbuf_fragment>
	#include <map_particle_fragment>
	#include <color_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	outgoingLight = diffuseColor.rgb;
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
	#include <premultiplied_alpha_fragment>
}`,shadow_vert:`#include <common>
#include <batching_pars_vertex>
#include <fog_pars_vertex>
#include <morphtarget_pars_vertex>
#include <skinning_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <shadowmap_pars_vertex>
void main() {
	#include <batching_vertex>
	#include <beginnormal_vertex>
	#include <morphinstance_vertex>
	#include <morphnormal_vertex>
	#include <skinbase_vertex>
	#include <skinnormal_vertex>
	#include <defaultnormal_vertex>
	#include <begin_vertex>
	#include <morphtarget_vertex>
	#include <skinning_vertex>
	#include <project_vertex>
	#include <logdepthbuf_vertex>
	#include <worldpos_vertex>
	#include <shadowmap_vertex>
	#include <fog_vertex>
}`,shadow_frag:`uniform vec3 color;
uniform float opacity;
#include <common>
#include <packing>
#include <fog_pars_fragment>
#include <bsdfs>
#include <lights_pars_begin>
#include <logdepthbuf_pars_fragment>
#include <shadowmap_pars_fragment>
#include <shadowmask_pars_fragment>
void main() {
	#include <logdepthbuf_fragment>
	gl_FragColor = vec4( color, opacity * ( 1.0 - getShadowMask() ) );
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
}`,sprite_vert:`uniform float rotation;
uniform vec2 center;
#include <common>
#include <uv_pars_vertex>
#include <fog_pars_vertex>
#include <logdepthbuf_pars_vertex>
#include <clipping_planes_pars_vertex>
void main() {
	#include <uv_vertex>
	vec4 mvPosition = modelViewMatrix * vec4( 0.0, 0.0, 0.0, 1.0 );
	vec2 scale;
	scale.x = length( vec3( modelMatrix[ 0 ].x, modelMatrix[ 0 ].y, modelMatrix[ 0 ].z ) );
	scale.y = length( vec3( modelMatrix[ 1 ].x, modelMatrix[ 1 ].y, modelMatrix[ 1 ].z ) );
	#ifndef USE_SIZEATTENUATION
		bool isPerspective = isPerspectiveMatrix( projectionMatrix );
		if ( isPerspective ) scale *= - mvPosition.z;
	#endif
	vec2 alignedPosition = ( position.xy - ( center - vec2( 0.5 ) ) ) * scale;
	vec2 rotatedPosition;
	rotatedPosition.x = cos( rotation ) * alignedPosition.x - sin( rotation ) * alignedPosition.y;
	rotatedPosition.y = sin( rotation ) * alignedPosition.x + cos( rotation ) * alignedPosition.y;
	mvPosition.xy += rotatedPosition;
	gl_Position = projectionMatrix * mvPosition;
	#include <logdepthbuf_vertex>
	#include <clipping_planes_vertex>
	#include <fog_vertex>
}`,sprite_frag:`uniform vec3 diffuse;
uniform float opacity;
#include <common>
#include <uv_pars_fragment>
#include <map_pars_fragment>
#include <alphamap_pars_fragment>
#include <alphatest_pars_fragment>
#include <alphahash_pars_fragment>
#include <fog_pars_fragment>
#include <logdepthbuf_pars_fragment>
#include <clipping_planes_pars_fragment>
void main() {
	vec4 diffuseColor = vec4( diffuse, opacity );
	#include <clipping_planes_fragment>
	vec3 outgoingLight = vec3( 0.0 );
	#include <logdepthbuf_fragment>
	#include <map_fragment>
	#include <alphamap_fragment>
	#include <alphatest_fragment>
	#include <alphahash_fragment>
	outgoingLight = diffuseColor.rgb;
	#include <opaque_fragment>
	#include <tonemapping_fragment>
	#include <colorspace_fragment>
	#include <fog_fragment>
}`},$={common:{diffuse:{value:new Sa(16777215)},opacity:{value:1},map:{value:null},mapTransform:{value:new X},alphaMap:{value:null},alphaMapTransform:{value:new X},alphaTest:{value:0}},specularmap:{specularMap:{value:null},specularMapTransform:{value:new X}},envmap:{envMap:{value:null},envMapRotation:{value:new X},flipEnvMap:{value:-1},reflectivity:{value:1},ior:{value:1.5},refractionRatio:{value:.98}},aomap:{aoMap:{value:null},aoMapIntensity:{value:1},aoMapTransform:{value:new X}},lightmap:{lightMap:{value:null},lightMapIntensity:{value:1},lightMapTransform:{value:new X}},bumpmap:{bumpMap:{value:null},bumpMapTransform:{value:new X},bumpScale:{value:1}},normalmap:{normalMap:{value:null},normalMapTransform:{value:new X},normalScale:{value:new Y(1,1)}},displacementmap:{displacementMap:{value:null},displacementMapTransform:{value:new X},displacementScale:{value:1},displacementBias:{value:0}},emissivemap:{emissiveMap:{value:null},emissiveMapTransform:{value:new X}},metalnessmap:{metalnessMap:{value:null},metalnessMapTransform:{value:new X}},roughnessmap:{roughnessMap:{value:null},roughnessMapTransform:{value:new X}},gradientmap:{gradientMap:{value:null}},fog:{fogDensity:{value:25e-5},fogNear:{value:1},fogFar:{value:2e3},fogColor:{value:new Sa(16777215)}},lights:{ambientLightColor:{value:[]},lightProbe:{value:[]},directionalLights:{value:[],properties:{direction:{},color:{}}},directionalLightShadows:{value:[],properties:{shadowBias:{},shadowNormalBias:{},shadowRadius:{},shadowMapSize:{}}},directionalShadowMap:{value:[]},directionalShadowMatrix:{value:[]},spotLights:{value:[],properties:{color:{},position:{},direction:{},distance:{},coneCos:{},penumbraCos:{},decay:{}}},spotLightShadows:{value:[],properties:{shadowBias:{},shadowNormalBias:{},shadowRadius:{},shadowMapSize:{}}},spotLightMap:{value:[]},spotShadowMap:{value:[]},spotLightMatrix:{value:[]},pointLights:{value:[],properties:{color:{},position:{},decay:{},distance:{}}},pointLightShadows:{value:[],properties:{shadowBias:{},shadowNormalBias:{},shadowRadius:{},shadowMapSize:{},shadowCameraNear:{},shadowCameraFar:{}}},pointShadowMap:{value:[]},pointShadowMatrix:{value:[]},hemisphereLights:{value:[],properties:{direction:{},skyColor:{},groundColor:{}}},rectAreaLights:{value:[],properties:{color:{},position:{},width:{},height:{}}},ltc_1:{value:null},ltc_2:{value:null}},points:{diffuse:{value:new Sa(16777215)},opacity:{value:1},size:{value:1},scale:{value:1},map:{value:null},alphaMap:{value:null},alphaMapTransform:{value:new X},alphaTest:{value:0},uvTransform:{value:new X}},sprite:{diffuse:{value:new Sa(16777215)},opacity:{value:1},center:{value:new Y(.5,.5)},rotation:{value:0},map:{value:null},mapTransform:{value:new X},alphaMap:{value:null},alphaMapTransform:{value:new X},alphaTest:{value:0}}},Lo={basic:{uniforms:lo([$.common,$.specularmap,$.envmap,$.aomap,$.lightmap,$.fog]),vertexShader:Q.meshbasic_vert,fragmentShader:Q.meshbasic_frag},lambert:{uniforms:lo([$.common,$.specularmap,$.envmap,$.aomap,$.lightmap,$.emissivemap,$.bumpmap,$.normalmap,$.displacementmap,$.fog,$.lights,{emissive:{value:new Sa(0)}}]),vertexShader:Q.meshlambert_vert,fragmentShader:Q.meshlambert_frag},phong:{uniforms:lo([$.common,$.specularmap,$.envmap,$.aomap,$.lightmap,$.emissivemap,$.bumpmap,$.normalmap,$.displacementmap,$.fog,$.lights,{emissive:{value:new Sa(0)},specular:{value:new Sa(1118481)},shininess:{value:30}}]),vertexShader:Q.meshphong_vert,fragmentShader:Q.meshphong_frag},standard:{uniforms:lo([$.common,$.envmap,$.aomap,$.lightmap,$.emissivemap,$.bumpmap,$.normalmap,$.displacementmap,$.roughnessmap,$.metalnessmap,$.fog,$.lights,{emissive:{value:new Sa(0)},roughness:{value:1},metalness:{value:0},envMapIntensity:{value:1}}]),vertexShader:Q.meshphysical_vert,fragmentShader:Q.meshphysical_frag},toon:{uniforms:lo([$.common,$.aomap,$.lightmap,$.emissivemap,$.bumpmap,$.normalmap,$.displacementmap,$.gradientmap,$.fog,$.lights,{emissive:{value:new Sa(0)}}]),vertexShader:Q.meshtoon_vert,fragmentShader:Q.meshtoon_frag},matcap:{uniforms:lo([$.common,$.bumpmap,$.normalmap,$.displacementmap,$.fog,{matcap:{value:null}}]),vertexShader:Q.meshmatcap_vert,fragmentShader:Q.meshmatcap_frag},points:{uniforms:lo([$.points,$.fog]),vertexShader:Q.points_vert,fragmentShader:Q.points_frag},dashed:{uniforms:lo([$.common,$.fog,{scale:{value:1},dashSize:{value:1},totalSize:{value:2}}]),vertexShader:Q.linedashed_vert,fragmentShader:Q.linedashed_frag},depth:{uniforms:lo([$.common,$.displacementmap]),vertexShader:Q.depth_vert,fragmentShader:Q.depth_frag},normal:{uniforms:lo([$.common,$.bumpmap,$.normalmap,$.displacementmap,{opacity:{value:1}}]),vertexShader:Q.meshnormal_vert,fragmentShader:Q.meshnormal_frag},sprite:{uniforms:lo([$.sprite,$.fog]),vertexShader:Q.sprite_vert,fragmentShader:Q.sprite_frag},background:{uniforms:{uvTransform:{value:new X},t2D:{value:null},backgroundIntensity:{value:1}},vertexShader:Q.background_vert,fragmentShader:Q.background_frag},backgroundCube:{uniforms:{envMap:{value:null},flipEnvMap:{value:-1},backgroundBlurriness:{value:0},backgroundIntensity:{value:1},backgroundRotation:{value:new X}},vertexShader:Q.backgroundCube_vert,fragmentShader:Q.backgroundCube_frag},cube:{uniforms:{tCube:{value:null},tFlip:{value:-1},opacity:{value:1}},vertexShader:Q.cube_vert,fragmentShader:Q.cube_frag},equirect:{uniforms:{tEquirect:{value:null}},vertexShader:Q.equirect_vert,fragmentShader:Q.equirect_frag},distanceRGBA:{uniforms:lo([$.common,$.displacementmap,{referencePosition:{value:new Z},nearDistance:{value:1},farDistance:{value:1e3}}]),vertexShader:Q.distanceRGBA_vert,fragmentShader:Q.distanceRGBA_frag},shadow:{uniforms:lo([$.lights,$.fog,{color:{value:new Sa(0)},opacity:{value:1}}]),vertexShader:Q.shadow_vert,fragmentShader:Q.shadow_frag}};Lo.physical={uniforms:lo([Lo.standard.uniforms,{clearcoat:{value:0},clearcoatMap:{value:null},clearcoatMapTransform:{value:new X},clearcoatNormalMap:{value:null},clearcoatNormalMapTransform:{value:new X},clearcoatNormalScale:{value:new Y(1,1)},clearcoatRoughness:{value:0},clearcoatRoughnessMap:{value:null},clearcoatRoughnessMapTransform:{value:new X},iridescence:{value:0},iridescenceMap:{value:null},iridescenceMapTransform:{value:new X},iridescenceIOR:{value:1.3},iridescenceThicknessMinimum:{value:100},iridescenceThicknessMaximum:{value:400},iridescenceThicknessMap:{value:null},iridescenceThicknessMapTransform:{value:new X},sheen:{value:0},sheenColor:{value:new Sa(0)},sheenColorMap:{value:null},sheenColorMapTransform:{value:new X},sheenRoughness:{value:1},sheenRoughnessMap:{value:null},sheenRoughnessMapTransform:{value:new X},transmission:{value:0},transmissionMap:{value:null},transmissionMapTransform:{value:new X},transmissionSamplerSize:{value:new Y},transmissionSamplerMap:{value:null},thickness:{value:0},thicknessMap:{value:null},thicknessMapTransform:{value:new X},attenuationDistance:{value:0},attenuationColor:{value:new Sa(0)},specularColor:{value:new Sa(1,1,1)},specularColorMap:{value:null},specularColorMapTransform:{value:new X},specularIntensity:{value:1},specularIntensityMap:{value:null},specularIntensityMapTransform:{value:new X},anisotropyVector:{value:new Y},anisotropyMap:{value:null},anisotropyMapTransform:{value:new X}}]),vertexShader:Q.meshphysical_vert,fragmentShader:Q.meshphysical_frag};var Ro={r:0,b:0,g:0},zo=new Ui,Bo=new Ni;function Vo(e,t,n,r,i,a,o){let s=new Sa(0),c=a===!0?0:1,l,u,d=null,f=0,p=null;function m(a,m){let g=!1,_=m.isScene===!0?m.background:null;_&&_.isTexture&&(_=(m.backgroundBlurriness>0?n:t).get(_)),_===null?h(s,c):_&&_.isColor&&(h(_,1),g=!0);let v=e.xr.getEnvironmentBlendMode();v===`additive`?r.buffers.color.setClear(0,0,0,1,o):v===`alpha-blend`&&r.buffers.color.setClear(0,0,0,0,o),(e.autoClear||g)&&e.clear(e.autoClearColor,e.autoClearDepth,e.autoClearStencil),_&&(_.isCubeTexture||_.mapping===306)?(u===void 0&&(u=new io(new so(1,1,1),new go({name:`BackgroundCubeMaterial`,uniforms:co(Lo.backgroundCube.uniforms),vertexShader:Lo.backgroundCube.vertexShader,fragmentShader:Lo.backgroundCube.fragmentShader,side:1,depthTest:!1,depthWrite:!1,fog:!1})),u.geometry.deleteAttribute(`normal`),u.geometry.deleteAttribute(`uv`),u.onBeforeRender=function(e,t,n){this.matrixWorld.copyPosition(n.matrixWorld)},Object.defineProperty(u.material,`envMap`,{get:function(){return this.uniforms.envMap.value}}),i.update(u)),zo.copy(m.backgroundRotation),zo.x*=-1,zo.y*=-1,zo.z*=-1,_.isCubeTexture&&_.isRenderTargetTexture===!1&&(zo.y*=-1,zo.z*=-1),u.material.uniforms.envMap.value=_,u.material.uniforms.flipEnvMap.value=_.isCubeTexture&&_.isRenderTargetTexture===!1?-1:1,u.material.uniforms.backgroundBlurriness.value=m.backgroundBlurriness,u.material.uniforms.backgroundIntensity.value=m.backgroundIntensity,u.material.uniforms.backgroundRotation.value.setFromMatrix4(Bo.makeRotationFromEuler(zo)),u.material.toneMapped=Hr.getTransfer(_.colorSpace)!==Qn,(d!==_||f!==_.version||p!==e.toneMapping)&&(u.material.needsUpdate=!0,d=_,f=_.version,p=e.toneMapping),u.layers.enableAll(),a.unshift(u,u.geometry,u.material,0,0,null)):_&&_.isTexture&&(l===void 0&&(l=new io(new Io(2,2),new go({name:`BackgroundMaterial`,uniforms:co(Lo.background.uniforms),vertexShader:Lo.background.vertexShader,fragmentShader:Lo.background.fragmentShader,side:0,depthTest:!1,depthWrite:!1,fog:!1})),l.geometry.deleteAttribute(`normal`),Object.defineProperty(l.material,`map`,{get:function(){return this.uniforms.t2D.value}}),i.update(l)),l.material.uniforms.t2D.value=_,l.material.uniforms.backgroundIntensity.value=m.backgroundIntensity,l.material.toneMapped=Hr.getTransfer(_.colorSpace)!==Qn,_.matrixAutoUpdate===!0&&_.updateMatrix(),l.material.uniforms.uvTransform.value.copy(_.matrix),(d!==_||f!==_.version||p!==e.toneMapping)&&(l.material.needsUpdate=!0,d=_,f=_.version,p=e.toneMapping),l.layers.enableAll(),a.unshift(l,l.geometry,l.material,0,0,null))}function h(t,n){t.getRGB(Ro,fo(e)),r.buffers.color.setClear(Ro.r,Ro.g,Ro.b,n,o)}return{getClearColor:function(){return s},setClearColor:function(e,t=1){s.set(e),c=t,h(s,c)},getClearAlpha:function(){return c},setClearAlpha:function(e){c=e,h(s,c)},render:m}}function Ho(e,t,n,r){let i=e.getParameter(e.MAX_VERTEX_ATTRIBS),a=r.isWebGL2?null:t.get(`OES_vertex_array_object`),o=r.isWebGL2||a!==null,s={},c=g(null),l=c,u=!1;function d(t,r,i,a,s){let c=!1;if(o){let e=h(a,i,r);l!==e&&(l=e,p(l.object)),c=_(t,a,i,s),c&&v(t,a,i,s)}else{let e=r.wireframe===!0;(l.geometry!==a.id||l.program!==i.id||l.wireframe!==e)&&(l.geometry=a.id,l.program=i.id,l.wireframe=e,c=!0)}s!==null&&n.update(s,e.ELEMENT_ARRAY_BUFFER),(c||u)&&(u=!1,w(t,r,i,a),s!==null&&e.bindBuffer(e.ELEMENT_ARRAY_BUFFER,n.get(s).buffer))}function f(){return r.isWebGL2?e.createVertexArray():a.createVertexArrayOES()}function p(t){return r.isWebGL2?e.bindVertexArray(t):a.bindVertexArrayOES(t)}function m(t){return r.isWebGL2?e.deleteVertexArray(t):a.deleteVertexArrayOES(t)}function h(e,t,n){let r=n.wireframe===!0,i=s[e.id];i===void 0&&(i={},s[e.id]=i);let a=i[t.id];a===void 0&&(a={},i[t.id]=a);let o=a[r];return o===void 0&&(o=g(f()),a[r]=o),o}function g(e){let t=[],n=[],r=[];for(let e=0;e<i;e++)t[e]=0,n[e]=0,r[e]=0;return{geometry:null,program:null,wireframe:!1,newAttributes:t,enabledAttributes:n,attributeDivisors:r,object:e,attributes:{},index:null}}function _(e,t,n,r){let i=l.attributes,a=t.attributes,o=0,s=n.getAttributes();for(let t in s)if(s[t].location>=0){let n=i[t],r=a[t];if(r===void 0&&(t===`instanceMatrix`&&e.instanceMatrix&&(r=e.instanceMatrix),t===`instanceColor`&&e.instanceColor&&(r=e.instanceColor)),n===void 0||n.attribute!==r||r&&n.data!==r.data)return!0;o++}return l.attributesNum!==o||l.index!==r}function v(e,t,n,r){let i={},a=t.attributes,o=0,s=n.getAttributes();for(let t in s)if(s[t].location>=0){let n=a[t];n===void 0&&(t===`instanceMatrix`&&e.instanceMatrix&&(n=e.instanceMatrix),t===`instanceColor`&&e.instanceColor&&(n=e.instanceColor));let r={};r.attribute=n,n&&n.data&&(r.data=n.data),i[t]=r,o++}l.attributes=i,l.attributesNum=o,l.index=r}function y(){let e=l.newAttributes;for(let t=0,n=e.length;t<n;t++)e[t]=0}function b(e){x(e,0)}function x(n,i){let a=l.newAttributes,o=l.enabledAttributes,s=l.attributeDivisors;a[n]=1,o[n]===0&&(e.enableVertexAttribArray(n),o[n]=1),s[n]!==i&&((r.isWebGL2?e:t.get(`ANGLE_instanced_arrays`))[r.isWebGL2?`vertexAttribDivisor`:`vertexAttribDivisorANGLE`](n,i),s[n]=i)}function S(){let t=l.newAttributes,n=l.enabledAttributes;for(let r=0,i=n.length;r<i;r++)n[r]!==t[r]&&(e.disableVertexAttribArray(r),n[r]=0)}function C(t,n,r,i,a,o,s){s===!0?e.vertexAttribIPointer(t,n,r,a,o):e.vertexAttribPointer(t,n,r,i,a,o)}function w(i,a,o,s){if(r.isWebGL2===!1&&(i.isInstancedMesh||s.isInstancedBufferGeometry)&&t.get(`ANGLE_instanced_arrays`)===null)return;y();let c=s.attributes,l=o.getAttributes(),u=a.defaultAttributeValues;for(let t in l){let a=l[t];if(a.location>=0){let o=c[t];if(o===void 0&&(t===`instanceMatrix`&&i.instanceMatrix&&(o=i.instanceMatrix),t===`instanceColor`&&i.instanceColor&&(o=i.instanceColor)),o!==void 0){let t=o.normalized,c=o.itemSize,l=n.get(o);if(l===void 0)continue;let u=l.buffer,d=l.type,f=l.bytesPerElement,p=r.isWebGL2===!0&&(d===e.INT||d===e.UNSIGNED_INT||o.gpuType===1013);if(o.isInterleavedBufferAttribute){let n=o.data,r=n.stride,l=o.offset;if(n.isInstancedInterleavedBuffer){for(let e=0;e<a.locationSize;e++)x(a.location+e,n.meshPerAttribute);i.isInstancedMesh!==!0&&s._maxInstanceCount===void 0&&(s._maxInstanceCount=n.meshPerAttribute*n.count)}else for(let e=0;e<a.locationSize;e++)b(a.location+e);e.bindBuffer(e.ARRAY_BUFFER,u);for(let e=0;e<a.locationSize;e++)C(a.location+e,c/a.locationSize,d,t,r*f,(l+c/a.locationSize*e)*f,p)}else{if(o.isInstancedBufferAttribute){for(let e=0;e<a.locationSize;e++)x(a.location+e,o.meshPerAttribute);i.isInstancedMesh!==!0&&s._maxInstanceCount===void 0&&(s._maxInstanceCount=o.meshPerAttribute*o.count)}else for(let e=0;e<a.locationSize;e++)b(a.location+e);e.bindBuffer(e.ARRAY_BUFFER,u);for(let e=0;e<a.locationSize;e++)C(a.location+e,c/a.locationSize,d,t,c*f,c/a.locationSize*e*f,p)}}else if(u!==void 0){let n=u[t];if(n!==void 0)switch(n.length){case 2:e.vertexAttrib2fv(a.location,n);break;case 3:e.vertexAttrib3fv(a.location,n);break;case 4:e.vertexAttrib4fv(a.location,n);break;default:e.vertexAttrib1fv(a.location,n)}}}}S()}function T(){O();for(let e in s){let t=s[e];for(let e in t){let n=t[e];for(let e in n)m(n[e].object),delete n[e];delete t[e]}delete s[e]}}function E(e){if(s[e.id]===void 0)return;let t=s[e.id];for(let e in t){let n=t[e];for(let e in n)m(n[e].object),delete n[e];delete t[e]}delete s[e.id]}function D(e){for(let t in s){let n=s[t];if(n[e.id]===void 0)continue;let r=n[e.id];for(let e in r)m(r[e].object),delete r[e];delete n[e.id]}}function O(){k(),u=!0,l!==c&&(l=c,p(l.object))}function k(){c.geometry=null,c.program=null,c.wireframe=!1}return{setup:d,reset:O,resetDefaultState:k,dispose:T,releaseStatesOfGeometry:E,releaseStatesOfProgram:D,initAttributes:y,enableAttribute:b,disableUnusedAttributes:S}}function Uo(e,t,n,r){let i=r.isWebGL2,a;function o(e){a=e}function s(t,r){e.drawArrays(a,t,r),n.update(r,a,1)}function c(r,o,s){if(s===0)return;let c,l;if(i)c=e,l=`drawArraysInstanced`;else if(c=t.get(`ANGLE_instanced_arrays`),l=`drawArraysInstancedANGLE`,c===null){console.error(`THREE.WebGLBufferRenderer: using THREE.InstancedBufferGeometry but hardware does not support extension ANGLE_instanced_arrays.`);return}c[l](a,r,o,s),n.update(o,a,s)}function l(e,r,i){if(i===0)return;let o=t.get(`WEBGL_multi_draw`);if(o===null)for(let t=0;t<i;t++)this.render(e[t],r[t]);else{o.multiDrawArraysWEBGL(a,e,0,r,0,i);let t=0;for(let e=0;e<i;e++)t+=r[e];n.update(t,a,1)}}this.setMode=o,this.render=s,this.renderInstances=c,this.renderMultiDraw=l}function Wo(e,t,n){let r;function i(){if(r!==void 0)return r;if(t.has(`EXT_texture_filter_anisotropic`)===!0){let n=t.get(`EXT_texture_filter_anisotropic`);r=e.getParameter(n.MAX_TEXTURE_MAX_ANISOTROPY_EXT)}else r=0;return r}function a(t){if(t===`highp`){if(e.getShaderPrecisionFormat(e.VERTEX_SHADER,e.HIGH_FLOAT).precision>0&&e.getShaderPrecisionFormat(e.FRAGMENT_SHADER,e.HIGH_FLOAT).precision>0)return`highp`;t=`mediump`}return t===`mediump`&&e.getShaderPrecisionFormat(e.VERTEX_SHADER,e.MEDIUM_FLOAT).precision>0&&e.getShaderPrecisionFormat(e.FRAGMENT_SHADER,e.MEDIUM_FLOAT).precision>0?`mediump`:`lowp`}let o=typeof WebGL2RenderingContext<`u`&&e.constructor.name===`WebGL2RenderingContext`,s=n.precision===void 0?`highp`:n.precision,c=a(s);c!==s&&(console.warn(`THREE.WebGLRenderer:`,s,`not supported, using`,c,`instead.`),s=c);let l=o||t.has(`WEBGL_draw_buffers`),u=n.logarithmicDepthBuffer===!0,d=e.getParameter(e.MAX_TEXTURE_IMAGE_UNITS),f=e.getParameter(e.MAX_VERTEX_TEXTURE_IMAGE_UNITS),p=e.getParameter(e.MAX_TEXTURE_SIZE),m=e.getParameter(e.MAX_CUBE_MAP_TEXTURE_SIZE),h=e.getParameter(e.MAX_VERTEX_ATTRIBS),g=e.getParameter(e.MAX_VERTEX_UNIFORM_VECTORS),_=e.getParameter(e.MAX_VARYING_VECTORS),v=e.getParameter(e.MAX_FRAGMENT_UNIFORM_VECTORS),y=f>0,b=o||t.has(`OES_texture_float`),x=y&&b,S=o?e.getParameter(e.MAX_SAMPLES):0;return{isWebGL2:o,drawBuffers:l,getMaxAnisotropy:i,getMaxPrecision:a,precision:s,logarithmicDepthBuffer:u,maxTextures:d,maxVertexTextures:f,maxTextureSize:p,maxCubemapSize:m,maxAttributes:h,maxVertexUniforms:g,maxVaryings:_,maxFragmentUniforms:v,vertexTextures:y,floatFragmentTextures:b,floatVertexTextures:x,maxSamples:S}}function Go(e){let t=this,n=null,r=0,i=!1,a=!1,o=new Ao,s=new X,c={value:null,needsUpdate:!1};this.uniform=c,this.numPlanes=0,this.numIntersection=0,this.init=function(e,t){let n=e.length!==0||t||r!==0||i;return i=t,r=e.length,n},this.beginShadows=function(){a=!0,u(null)},this.endShadows=function(){a=!1},this.setGlobalState=function(e,t){n=u(e,t,0)},this.setState=function(t,o,s){let d=t.clippingPlanes,f=t.clipIntersection,p=t.clipShadows,m=e.get(t);if(!i||d===null||d.length===0||a&&!p)a?u(null):l();else{let e=a?0:r,t=e*4,i=m.clippingState||null;c.value=i,i=u(d,o,t,s);for(let e=0;e!==t;++e)i[e]=n[e];m.clippingState=i,this.numIntersection=f?this.numPlanes:0,this.numPlanes+=e}};function l(){c.value!==n&&(c.value=n,c.needsUpdate=r>0),t.numPlanes=r,t.numIntersection=0}function u(e,n,r,i){let a=e===null?0:e.length,l=null;if(a!==0){if(l=c.value,i!==!0||l===null){let t=r+a*4,i=n.matrixWorldInverse;s.getNormalMatrix(i),(l===null||l.length<t)&&(l=new Float32Array(t));for(let t=0,n=r;t!==a;++t,n+=4)o.copy(e[t]).applyMatrix4(i,s),o.normal.toArray(l,n),l[n+3]=o.constant}c.value=l,c.needsUpdate=!0}return t.numPlanes=a,t.numIntersection=0,l}}function Ko(e){let t=new WeakMap;function n(e,t){return t===303?e.mapping=301:t===304&&(e.mapping=302),e}function r(r){if(r&&r.isTexture){let a=r.mapping;if(a===303||a===304)if(t.has(r)){let e=t.get(r).texture;return n(e,r.mapping)}else{let a=r.image;if(a&&a.height>0){let o=new Eo(a.height);return o.fromEquirectangularTexture(e,r),t.set(r,o),r.addEventListener(`dispose`,i),n(o.texture,r.mapping)}else return null}}return r}function i(e){let n=e.target;n.removeEventListener(`dispose`,i);let r=t.get(n);r!==void 0&&(t.delete(n),r.dispose())}function a(){t=new WeakMap}return{get:r,dispose:a}}var qo=class extends _o{constructor(e=-1,t=1,n=1,r=-1,i=.1,a=2e3){super(),this.isOrthographicCamera=!0,this.type=`OrthographicCamera`,this.zoom=1,this.view=null,this.left=e,this.right=t,this.top=n,this.bottom=r,this.near=i,this.far=a,this.updateProjectionMatrix()}copy(e,t){return super.copy(e,t),this.left=e.left,this.right=e.right,this.top=e.top,this.bottom=e.bottom,this.near=e.near,this.far=e.far,this.zoom=e.zoom,this.view=e.view===null?null:Object.assign({},e.view),this}setViewOffset(e,t,n,r,i,a){this.view===null&&(this.view={enabled:!0,fullWidth:1,fullHeight:1,offsetX:0,offsetY:0,width:1,height:1}),this.view.enabled=!0,this.view.fullWidth=e,this.view.fullHeight=t,this.view.offsetX=n,this.view.offsetY=r,this.view.width=i,this.view.height=a,this.updateProjectionMatrix()}clearViewOffset(){this.view!==null&&(this.view.enabled=!1),this.updateProjectionMatrix()}updateProjectionMatrix(){let e=(this.right-this.left)/(2*this.zoom),t=(this.top-this.bottom)/(2*this.zoom),n=(this.right+this.left)/2,r=(this.top+this.bottom)/2,i=n-e,a=n+e,o=r+t,s=r-t;if(this.view!==null&&this.view.enabled){let e=(this.right-this.left)/this.view.fullWidth/this.zoom,t=(this.top-this.bottom)/this.view.fullHeight/this.zoom;i+=e*this.view.offsetX,a=i+e*this.view.width,o-=t*this.view.offsetY,s=o-t*this.view.height}this.projectionMatrix.makeOrthographic(i,a,o,s,this.near,this.far,this.coordinateSystem),this.projectionMatrixInverse.copy(this.projectionMatrix).invert()}toJSON(e){let t=super.toJSON(e);return t.object.zoom=this.zoom,t.object.left=this.left,t.object.right=this.right,t.object.top=this.top,t.object.bottom=this.bottom,t.object.near=this.near,t.object.far=this.far,this.view!==null&&(t.object.view=Object.assign({},this.view)),t}},Jo=4,Yo=[.125,.215,.35,.446,.526,.582],Xo=20,Zo=new qo,Qo=new Sa,$o=null,es=0,ts=0,ns=(1+Math.sqrt(5))/2,rs=1/ns,is=[new Z(1,1,1),new Z(-1,1,1),new Z(1,1,-1),new Z(-1,1,-1),new Z(0,ns,rs),new Z(0,ns,-rs),new Z(rs,0,ns),new Z(-rs,0,ns),new Z(ns,rs,0),new Z(-ns,rs,0)],as=class{constructor(e){this._renderer=e,this._pingPongRenderTarget=null,this._lodMax=0,this._cubeSize=0,this._lodPlanes=[],this._sizeLods=[],this._sigmas=[],this._blurMaterial=null,this._cubemapMaterial=null,this._equirectMaterial=null,this._compileMaterial(this._blurMaterial)}fromScene(e,t=0,n=.1,r=100){$o=this._renderer.getRenderTarget(),es=this._renderer.getActiveCubeFace(),ts=this._renderer.getActiveMipmapLevel(),this._setSize(256);let i=this._allocateTargets();return i.depthBuffer=!0,this._sceneToCubeUV(e,n,r,i),t>0&&this._blur(i,0,0,t),this._applyPMREM(i),this._cleanup(i),i}fromEquirectangular(e,t=null){return this._fromTexture(e,t)}fromCubemap(e,t=null){return this._fromTexture(e,t)}compileCubemapShader(){this._cubemapMaterial===null&&(this._cubemapMaterial=ds(),this._compileMaterial(this._cubemapMaterial))}compileEquirectangularShader(){this._equirectMaterial===null&&(this._equirectMaterial=us(),this._compileMaterial(this._equirectMaterial))}dispose(){this._dispose(),this._cubemapMaterial!==null&&this._cubemapMaterial.dispose(),this._equirectMaterial!==null&&this._equirectMaterial.dispose()}_setSize(e){this._lodMax=Math.floor(Math.log2(e)),this._cubeSize=2**this._lodMax}_dispose(){this._blurMaterial!==null&&this._blurMaterial.dispose(),this._pingPongRenderTarget!==null&&this._pingPongRenderTarget.dispose();for(let e=0;e<this._lodPlanes.length;e++)this._lodPlanes[e].dispose()}_cleanup(e){this._renderer.setRenderTarget($o,es,ts),e.scissorTest=!1,cs(e,0,0,e.width,e.height)}_fromTexture(e,t){e.mapping===301||e.mapping===302?this._setSize(e.image.length===0?16:e.image[0].width||e.image[0].image.width):this._setSize(e.image.width/4),$o=this._renderer.getRenderTarget(),es=this._renderer.getActiveCubeFace(),ts=this._renderer.getActiveMipmapLevel();let n=t||this._allocateTargets();return this._textureToCubeUV(e,n),this._applyPMREM(n),this._cleanup(n),n}_allocateTargets(){let e=3*Math.max(this._cubeSize,112),t=4*this._cubeSize,n={magFilter:On,minFilter:On,generateMipmaps:!1,type:Pn,format:In,colorSpace:Jn,depthBuffer:!1},r=ss(e,t,n);if(this._pingPongRenderTarget===null||this._pingPongRenderTarget.width!==e||this._pingPongRenderTarget.height!==t){this._pingPongRenderTarget!==null&&this._dispose(),this._pingPongRenderTarget=ss(e,t,n);let{_lodMax:r}=this;({sizeLods:this._sizeLods,lodPlanes:this._lodPlanes,sigmas:this._sigmas}=os(r)),this._blurMaterial=ls(r,e,t)}return r}_compileMaterial(e){let t=new io(this._lodPlanes[0],e);this._renderer.compile(t,Zo)}_sceneToCubeUV(e,t,n,r){let i=new xo(90,1,t,n),a=[1,-1,1,1,1,1],o=[1,1,1,-1,-1,-1],s=this._renderer,c=s.autoClear,l=s.toneMapping;s.getClearColor(Qo),s.toneMapping=0,s.autoClear=!1;let u=new Ea({name:`PMREM.Background`,side:1,depthWrite:!1,depthTest:!1}),d=new io(new so,u),f=!1,p=e.background;p?p.isColor&&(u.color.copy(p),e.background=null,f=!0):(u.color.copy(Qo),f=!0);for(let t=0;t<6;t++){let n=t%3;n===0?(i.up.set(0,a[t],0),i.lookAt(o[t],0,0)):n===1?(i.up.set(0,0,a[t]),i.lookAt(0,o[t],0)):(i.up.set(0,a[t],0),i.lookAt(0,0,o[t]));let c=this._cubeSize;cs(r,n*c,t>2?c:0,c,c),s.setRenderTarget(r),f&&s.render(d,i),s.render(e,i)}d.geometry.dispose(),d.material.dispose(),s.toneMapping=l,s.autoClear=c,e.background=p}_textureToCubeUV(e,t){let n=this._renderer,r=e.mapping===301||e.mapping===302;r?(this._cubemapMaterial===null&&(this._cubemapMaterial=ds()),this._cubemapMaterial.uniforms.flipEnvMap.value=e.isRenderTargetTexture===!1?-1:1):this._equirectMaterial===null&&(this._equirectMaterial=us());let i=r?this._cubemapMaterial:this._equirectMaterial,a=new io(this._lodPlanes[0],i),o=i.uniforms;o.envMap.value=e;let s=this._cubeSize;cs(t,0,0,3*s,2*s),n.setRenderTarget(t),n.render(a,Zo)}_applyPMREM(e){let t=this._renderer,n=t.autoClear;t.autoClear=!1;for(let t=1;t<this._lodPlanes.length;t++){let n=Math.sqrt(this._sigmas[t]*this._sigmas[t]-this._sigmas[t-1]*this._sigmas[t-1]),r=is[(t-1)%is.length];this._blur(e,t-1,t,n,r)}t.autoClear=n}_blur(e,t,n,r,i){let a=this._pingPongRenderTarget;this._halfBlur(e,a,t,n,r,`latitudinal`,i),this._halfBlur(a,e,n,n,r,`longitudinal`,i)}_halfBlur(e,t,n,r,i,a,o){let s=this._renderer,c=this._blurMaterial;a!==`latitudinal`&&a!==`longitudinal`&&console.error(`blur direction must be either latitudinal or longitudinal!`);let l=new io(this._lodPlanes[r],c),u=c.uniforms,d=this._sizeLods[n]-1,f=isFinite(i)?Math.PI/(2*d):2*Math.PI/(2*Xo-1),p=i/f,m=isFinite(i)?1+Math.floor(3*p):Xo;m>Xo&&console.warn(`sigmaRadians, ${i}, is too large and will clip, as it requested ${m} samples when the maximum is set to ${Xo}`);let h=[],g=0;for(let e=0;e<Xo;++e){let t=e/p,n=Math.exp(-t*t/2);h.push(n),e===0?g+=n:e<m&&(g+=2*n)}for(let e=0;e<h.length;e++)h[e]=h[e]/g;u.envMap.value=e.texture,u.samples.value=m,u.weights.value=h,u.latitudinal.value=a===`latitudinal`,o&&(u.poleAxis.value=o);let{_lodMax:_}=this;u.dTheta.value=f,u.mipInt.value=_-n;let v=this._sizeLods[r];cs(t,3*v*(r>_-Jo?r-_+Jo:0),4*(this._cubeSize-v),3*v,2*v),s.setRenderTarget(t),s.render(l,Zo)}};function os(e){let t=[],n=[],r=[],i=e,a=e-Jo+1+Yo.length;for(let o=0;o<a;o++){let a=2**i;n.push(a);let s=1/a;o>e-Jo?s=Yo[o-e+Jo-1]:o===0&&(s=0),r.push(s);let c=1/(a-2),l=-c,u=1+c,d=[l,l,u,l,u,u,l,l,u,u,l,u],f=new Float32Array(108),p=new Float32Array(72),m=new Float32Array(36);for(let e=0;e<6;e++){let t=e%3*2/3-1,n=e>2?0:-1,r=[t,n,0,t+2/3,n,0,t+2/3,n+1,0,t,n,0,t+2/3,n+1,0,t,n+1,0];f.set(r,18*e),p.set(d,12*e);let i=[e,e,e,e,e,e];m.set(i,6*e)}let h=new Ba;h.setAttribute(`position`,new ka(f,3)),h.setAttribute(`uv`,new ka(p,2)),h.setAttribute(`faceIndex`,new ka(m,1)),t.push(h),i>Jo&&i--}return{lodPlanes:t,sizeLods:n,sigmas:r}}function ss(e,t,n){let r=new ei(e,t,n);return r.texture.mapping=306,r.texture.name=`PMREM.cubeUv`,r.scissorTest=!0,r}function cs(e,t,n,r,i){e.viewport.set(t,n,r,i),e.scissor.set(t,n,r,i)}function ls(e,t,n){let r=new Float32Array(Xo),i=new Z(0,1,0);return new go({name:`SphericalGaussianBlur`,defines:{n:Xo,CUBEUV_TEXEL_WIDTH:1/t,CUBEUV_TEXEL_HEIGHT:1/n,CUBEUV_MAX_MIP:`${e}.0`},uniforms:{envMap:{value:null},samples:{value:1},weights:{value:r},latitudinal:{value:!1},dTheta:{value:0},mipInt:{value:0},poleAxis:{value:i}},vertexShader:fs(),fragmentShader:`

			precision mediump float;
			precision mediump int;

			varying vec3 vOutputDirection;

			uniform sampler2D envMap;
			uniform int samples;
			uniform float weights[ n ];
			uniform bool latitudinal;
			uniform float dTheta;
			uniform float mipInt;
			uniform vec3 poleAxis;

			#define ENVMAP_TYPE_CUBE_UV
			#include <cube_uv_reflection_fragment>

			vec3 getSample( float theta, vec3 axis ) {

				float cosTheta = cos( theta );
				// Rodrigues' axis-angle rotation
				vec3 sampleDirection = vOutputDirection * cosTheta
					+ cross( axis, vOutputDirection ) * sin( theta )
					+ axis * dot( axis, vOutputDirection ) * ( 1.0 - cosTheta );

				return bilinearCubeUV( envMap, sampleDirection, mipInt );

			}

			void main() {

				vec3 axis = latitudinal ? poleAxis : cross( poleAxis, vOutputDirection );

				if ( all( equal( axis, vec3( 0.0 ) ) ) ) {

					axis = vec3( vOutputDirection.z, 0.0, - vOutputDirection.x );

				}

				axis = normalize( axis );

				gl_FragColor = vec4( 0.0, 0.0, 0.0, 1.0 );
				gl_FragColor.rgb += weights[ 0 ] * getSample( 0.0, axis );

				for ( int i = 1; i < n; i++ ) {

					if ( i >= samples ) {

						break;

					}

					float theta = dTheta * float( i );
					gl_FragColor.rgb += weights[ i ] * getSample( -1.0 * theta, axis );
					gl_FragColor.rgb += weights[ i ] * getSample( theta, axis );

				}

			}
		`,blending:0,depthTest:!1,depthWrite:!1})}function us(){return new go({name:`EquirectangularToCubeUV`,uniforms:{envMap:{value:null}},vertexShader:fs(),fragmentShader:`

			precision mediump float;
			precision mediump int;

			varying vec3 vOutputDirection;

			uniform sampler2D envMap;

			#include <common>

			void main() {

				vec3 outputDirection = normalize( vOutputDirection );
				vec2 uv = equirectUv( outputDirection );

				gl_FragColor = vec4( texture2D ( envMap, uv ).rgb, 1.0 );

			}
		`,blending:0,depthTest:!1,depthWrite:!1})}function ds(){return new go({name:`CubemapToCubeUV`,uniforms:{envMap:{value:null},flipEnvMap:{value:-1}},vertexShader:fs(),fragmentShader:`

			precision mediump float;
			precision mediump int;

			uniform float flipEnvMap;

			varying vec3 vOutputDirection;

			uniform samplerCube envMap;

			void main() {

				gl_FragColor = textureCube( envMap, vec3( flipEnvMap * vOutputDirection.x, vOutputDirection.yz ) );

			}
		`,blending:0,depthTest:!1,depthWrite:!1})}function fs(){return`

		precision mediump float;
		precision mediump int;

		attribute float faceIndex;

		varying vec3 vOutputDirection;

		// RH coordinate system; PMREM face-indexing convention
		vec3 getDirection( vec2 uv, float face ) {

			uv = 2.0 * uv - 1.0;

			vec3 direction = vec3( uv, 1.0 );

			if ( face == 0.0 ) {

				direction = direction.zyx; // ( 1, v, u ) pos x

			} else if ( face == 1.0 ) {

				direction = direction.xzy;
				direction.xz *= -1.0; // ( -u, 1, -v ) pos y

			} else if ( face == 2.0 ) {

				direction.x *= -1.0; // ( -u, v, 1 ) pos z

			} else if ( face == 3.0 ) {

				direction = direction.zyx;
				direction.xz *= -1.0; // ( -1, v, -u ) neg x

			} else if ( face == 4.0 ) {

				direction = direction.xzy;
				direction.xy *= -1.0; // ( -u, -1, v ) neg y

			} else if ( face == 5.0 ) {

				direction.z *= -1.0; // ( u, v, -1 ) neg z

			}

			return direction;

		}

		void main() {

			vOutputDirection = getDirection( uv, faceIndex );
			gl_Position = vec4( position, 1.0 );

		}
	`}function ps(e){let t=new WeakMap,n=null;function r(r){if(r&&r.isTexture){let o=r.mapping,s=o===303||o===304,c=o===301||o===302;if(s||c)if(r.isRenderTargetTexture&&r.needsPMREMUpdate===!0){r.needsPMREMUpdate=!1;let i=t.get(r);return n===null&&(n=new as(e)),i=s?n.fromEquirectangular(r,i):n.fromCubemap(r,i),t.set(r,i),i.texture}else if(t.has(r))return t.get(r).texture;else{let o=r.image;if(s&&o&&o.height>0||c&&o&&i(o)){n===null&&(n=new as(e));let i=s?n.fromEquirectangular(r):n.fromCubemap(r);return t.set(r,i),r.addEventListener(`dispose`,a),i.texture}else return null}}return r}function i(e){let t=0;for(let n=0;n<6;n++)e[n]!==void 0&&t++;return t===6}function a(e){let n=e.target;n.removeEventListener(`dispose`,a);let r=t.get(n);r!==void 0&&(t.delete(n),r.dispose())}function o(){t=new WeakMap,n!==null&&(n.dispose(),n=null)}return{get:r,dispose:o}}function ms(e){let t={};function n(n){if(t[n]!==void 0)return t[n];let r;switch(n){case`WEBGL_depth_texture`:r=e.getExtension(`WEBGL_depth_texture`)||e.getExtension(`MOZ_WEBGL_depth_texture`)||e.getExtension(`WEBKIT_WEBGL_depth_texture`);break;case`EXT_texture_filter_anisotropic`:r=e.getExtension(`EXT_texture_filter_anisotropic`)||e.getExtension(`MOZ_EXT_texture_filter_anisotropic`)||e.getExtension(`WEBKIT_EXT_texture_filter_anisotropic`);break;case`WEBGL_compressed_texture_s3tc`:r=e.getExtension(`WEBGL_compressed_texture_s3tc`)||e.getExtension(`MOZ_WEBGL_compressed_texture_s3tc`)||e.getExtension(`WEBKIT_WEBGL_compressed_texture_s3tc`);break;case`WEBGL_compressed_texture_pvrtc`:r=e.getExtension(`WEBGL_compressed_texture_pvrtc`)||e.getExtension(`WEBKIT_WEBGL_compressed_texture_pvrtc`);break;default:r=e.getExtension(n)}return t[n]=r,r}return{has:function(e){return n(e)!==null},init:function(e){e.isWebGL2?(n(`EXT_color_buffer_float`),n(`WEBGL_clip_cull_distance`)):(n(`WEBGL_depth_texture`),n(`OES_texture_float`),n(`OES_texture_half_float`),n(`OES_texture_half_float_linear`),n(`OES_standard_derivatives`),n(`OES_element_index_uint`),n(`OES_vertex_array_object`),n(`ANGLE_instanced_arrays`)),n(`OES_texture_float_linear`),n(`EXT_color_buffer_half_float`),n(`WEBGL_multisampled_render_to_texture`)},get:function(e){let t=n(e);return t===null&&console.warn(`THREE.WebGLRenderer: `+e+` extension not supported.`),t}}}function hs(e,t,n,r){let i={},a=new WeakMap;function o(e){let s=e.target;s.index!==null&&t.remove(s.index);for(let e in s.attributes)t.remove(s.attributes[e]);for(let e in s.morphAttributes){let n=s.morphAttributes[e];for(let e=0,r=n.length;e<r;e++)t.remove(n[e])}s.removeEventListener(`dispose`,o),delete i[s.id];let c=a.get(s);c&&(t.remove(c),a.delete(s)),r.releaseStatesOfGeometry(s),s.isInstancedBufferGeometry===!0&&delete s._maxInstanceCount,n.memory.geometries--}function s(e,t){return i[t.id]===!0?t:(t.addEventListener(`dispose`,o),i[t.id]=!0,n.memory.geometries++,t)}function c(n){let r=n.attributes;for(let n in r)t.update(r[n],e.ARRAY_BUFFER);let i=n.morphAttributes;for(let n in i){let r=i[n];for(let n=0,i=r.length;n<i;n++)t.update(r[n],e.ARRAY_BUFFER)}}function l(e){let n=[],r=e.index,i=e.attributes.position,o=0;if(r!==null){let e=r.array;o=r.version;for(let t=0,r=e.length;t<r;t+=3){let r=e[t+0],i=e[t+1],a=e[t+2];n.push(r,i,i,a,a,r)}}else if(i!==void 0){let e=i.array;o=i.version;for(let t=0,r=e.length/3-1;t<r;t+=3){let e=t+0,r=t+1,i=t+2;n.push(e,r,r,i,i,e)}}else return;let s=new(Nr(n)?ja:Aa)(n,1);s.version=o;let c=a.get(e);c&&t.remove(c),a.set(e,s)}function u(e){let t=a.get(e);if(t){let n=e.index;n!==null&&t.version<n.version&&l(e)}else l(e);return a.get(e)}return{get:s,update:c,getWireframeAttribute:u}}function gs(e,t,n,r){let i=r.isWebGL2,a;function o(e){a=e}let s,c;function l(e){s=e.type,c=e.bytesPerElement}function u(t,r){e.drawElements(a,r,s,t*c),n.update(r,a,1)}function d(r,o,l){if(l===0)return;let u,d;if(i)u=e,d=`drawElementsInstanced`;else if(u=t.get(`ANGLE_instanced_arrays`),d=`drawElementsInstancedANGLE`,u===null){console.error(`THREE.WebGLIndexedBufferRenderer: using THREE.InstancedBufferGeometry but hardware does not support extension ANGLE_instanced_arrays.`);return}u[d](a,o,s,r*c,l),n.update(o,a,l)}function f(e,r,i){if(i===0)return;let o=t.get(`WEBGL_multi_draw`);if(o===null)for(let t=0;t<i;t++)this.render(e[t]/c,r[t]);else{o.multiDrawElementsWEBGL(a,r,0,s,e,0,i);let t=0;for(let e=0;e<i;e++)t+=r[e];n.update(t,a,1)}}this.setMode=o,this.setIndex=l,this.render=u,this.renderInstances=d,this.renderMultiDraw=f}function _s(e){let t={geometries:0,textures:0},n={frame:0,calls:0,triangles:0,points:0,lines:0};function r(t,r,i){switch(n.calls++,r){case e.TRIANGLES:n.triangles+=t/3*i;break;case e.LINES:n.lines+=t/2*i;break;case e.LINE_STRIP:n.lines+=i*(t-1);break;case e.LINE_LOOP:n.lines+=i*t;break;case e.POINTS:n.points+=i*t;break;default:console.error(`THREE.WebGLInfo: Unknown draw mode:`,r);break}}function i(){n.calls=0,n.triangles=0,n.points=0,n.lines=0}return{memory:t,render:n,programs:null,autoReset:!0,reset:i,update:r}}function vs(e,t){return e[0]-t[0]}function ys(e,t){return Math.abs(t[1])-Math.abs(e[1])}function bs(e,t,n){let r={},i=new Float32Array(8),a=new WeakMap,o=new Qr,s=[];for(let e=0;e<8;e++)s[e]=[e,0];function c(c,l,u){let d=c.morphTargetInfluences;if(t.isWebGL2===!0){let r=l.morphAttributes.position||l.morphAttributes.normal||l.morphAttributes.color,i=r===void 0?0:r.length,s=a.get(l);if(s===void 0||s.count!==i){s!==void 0&&s.texture.dispose();let e=l.morphAttributes.position!==void 0,n=l.morphAttributes.normal!==void 0,r=l.morphAttributes.color!==void 0,c=l.morphAttributes.position||[],u=l.morphAttributes.normal||[],d=l.morphAttributes.color||[],f=0;e===!0&&(f=1),n===!0&&(f=2),r===!0&&(f=3);let p=l.attributes.position.count*f,m=1;p>t.maxTextureSize&&(m=Math.ceil(p/t.maxTextureSize),p=t.maxTextureSize);let h=new Float32Array(p*m*4*i),g=new ti(h,p,m,i);g.type=Nn,g.needsUpdate=!0;let _=f*4;for(let t=0;t<i;t++){let i=c[t],a=u[t],s=d[t],l=p*m*4*t;for(let t=0;t<i.count;t++){let c=t*_;e===!0&&(o.fromBufferAttribute(i,t),h[l+c+0]=o.x,h[l+c+1]=o.y,h[l+c+2]=o.z,h[l+c+3]=0),n===!0&&(o.fromBufferAttribute(a,t),h[l+c+4]=o.x,h[l+c+5]=o.y,h[l+c+6]=o.z,h[l+c+7]=0),r===!0&&(o.fromBufferAttribute(s,t),h[l+c+8]=o.x,h[l+c+9]=o.y,h[l+c+10]=o.z,h[l+c+11]=s.itemSize===4?o.w:1)}}s={count:i,texture:g,size:new Y(p,m)},a.set(l,s);function v(){g.dispose(),a.delete(l),l.removeEventListener(`dispose`,v)}l.addEventListener(`dispose`,v)}if(c.isInstancedMesh===!0&&c.morphTexture!==null)u.getUniforms().setValue(e,`morphTexture`,c.morphTexture,n);else{let t=0;for(let e=0;e<d.length;e++)t+=d[e];let n=l.morphTargetsRelative?1:1-t;u.getUniforms().setValue(e,`morphTargetBaseInfluence`,n),u.getUniforms().setValue(e,`morphTargetInfluences`,d)}u.getUniforms().setValue(e,`morphTargetsTexture`,s.texture,n),u.getUniforms().setValue(e,`morphTargetsTextureSize`,s.size)}else{let t=d===void 0?0:d.length,n=r[l.id];if(n===void 0||n.length!==t){n=[];for(let e=0;e<t;e++)n[e]=[e,0];r[l.id]=n}for(let e=0;e<t;e++){let t=n[e];t[0]=e,t[1]=d[e]}n.sort(ys);for(let e=0;e<8;e++)e<t&&n[e][1]?(s[e][0]=n[e][0],s[e][1]=n[e][1]):(s[e][0]=2**53-1,s[e][1]=0);s.sort(vs);let a=l.morphAttributes.position,o=l.morphAttributes.normal,c=0;for(let e=0;e<8;e++){let t=s[e],n=t[0],r=t[1];n!==2**53-1&&r?(a&&l.getAttribute(`morphTarget`+e)!==a[n]&&l.setAttribute(`morphTarget`+e,a[n]),o&&l.getAttribute(`morphNormal`+e)!==o[n]&&l.setAttribute(`morphNormal`+e,o[n]),i[e]=r,c+=r):(a&&l.hasAttribute(`morphTarget`+e)===!0&&l.deleteAttribute(`morphTarget`+e),o&&l.hasAttribute(`morphNormal`+e)===!0&&l.deleteAttribute(`morphNormal`+e),i[e]=0)}let f=l.morphTargetsRelative?1:1-c;u.getUniforms().setValue(e,`morphTargetBaseInfluence`,f),u.getUniforms().setValue(e,`morphTargetInfluences`,i)}}return{update:c}}function xs(e,t,n,r){let i=new WeakMap;function a(a){let o=r.render.frame,c=a.geometry,l=t.get(a,c);if(i.get(l)!==o&&(t.update(l),i.set(l,o)),a.isInstancedMesh&&(a.hasEventListener(`dispose`,s)===!1&&a.addEventListener(`dispose`,s),i.get(a)!==o&&(n.update(a.instanceMatrix,e.ARRAY_BUFFER),a.instanceColor!==null&&n.update(a.instanceColor,e.ARRAY_BUFFER),i.set(a,o))),a.isSkinnedMesh){let e=a.skeleton;i.get(e)!==o&&(e.update(),i.set(e,o))}return l}function o(){i=new WeakMap}function s(e){let t=e.target;t.removeEventListener(`dispose`,s),n.remove(t.instanceMatrix),t.instanceColor!==null&&n.remove(t.instanceColor)}return{update:a,dispose:o}}var Ss=class extends Zr{constructor(e,t,n,r,i,a,o,s,c,l){if(l=l===void 0?Ln:l,l!==1026&&l!==1027)throw Error(`DepthTexture format must be either THREE.DepthFormat or THREE.DepthStencilFormat`);n===void 0&&l===1026&&(n=Mn),n===void 0&&l===1027&&(n=Fn),super(null,r,i,a,o,s,l,n,c),this.isDepthTexture=!0,this.image={width:e,height:t},this.magFilter=o===void 0?Tn:o,this.minFilter=s===void 0?Tn:s,this.flipY=!1,this.generateMipmaps=!1,this.compareFunction=null}copy(e){return super.copy(e),this.compareFunction=e.compareFunction,this}toJSON(e){let t=super.toJSON(e);return this.compareFunction!==null&&(t.compareFunction=this.compareFunction),t}},Cs=new Zr,ws=new Ss(1,1);ws.compareFunction=515;var Ts=new ti,Es=new ni,Ds=new To,Os=[],ks=[],As=new Float32Array(16),js=new Float32Array(9),Ms=new Float32Array(4);function Ns(e,t,n){let r=e[0];if(r<=0||r>0)return e;let i=t*n,a=Os[i];if(a===void 0&&(a=new Float32Array(i),Os[i]=a),t!==0){r.toArray(a,0);for(let r=1,i=0;r!==t;++r)i+=n,e[r].toArray(a,i)}return a}function Ps(e,t){if(e.length!==t.length)return!1;for(let n=0,r=e.length;n<r;n++)if(e[n]!==t[n])return!1;return!0}function Fs(e,t){for(let n=0,r=t.length;n<r;n++)e[n]=t[n]}function Is(e,t){let n=ks[t];n===void 0&&(n=new Int32Array(t),ks[t]=n);for(let r=0;r!==t;++r)n[r]=e.allocateTextureUnit();return n}function Ls(e,t){let n=this.cache;n[0]!==t&&(e.uniform1f(this.addr,t),n[0]=t)}function Rs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y)&&(e.uniform2f(this.addr,t.x,t.y),n[0]=t.x,n[1]=t.y);else{if(Ps(n,t))return;e.uniform2fv(this.addr,t),Fs(n,t)}}function zs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y||n[2]!==t.z)&&(e.uniform3f(this.addr,t.x,t.y,t.z),n[0]=t.x,n[1]=t.y,n[2]=t.z);else if(t.r!==void 0)(n[0]!==t.r||n[1]!==t.g||n[2]!==t.b)&&(e.uniform3f(this.addr,t.r,t.g,t.b),n[0]=t.r,n[1]=t.g,n[2]=t.b);else{if(Ps(n,t))return;e.uniform3fv(this.addr,t),Fs(n,t)}}function Bs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y||n[2]!==t.z||n[3]!==t.w)&&(e.uniform4f(this.addr,t.x,t.y,t.z,t.w),n[0]=t.x,n[1]=t.y,n[2]=t.z,n[3]=t.w);else{if(Ps(n,t))return;e.uniform4fv(this.addr,t),Fs(n,t)}}function Vs(e,t){let n=this.cache,r=t.elements;if(r===void 0){if(Ps(n,t))return;e.uniformMatrix2fv(this.addr,!1,t),Fs(n,t)}else{if(Ps(n,r))return;Ms.set(r),e.uniformMatrix2fv(this.addr,!1,Ms),Fs(n,r)}}function Hs(e,t){let n=this.cache,r=t.elements;if(r===void 0){if(Ps(n,t))return;e.uniformMatrix3fv(this.addr,!1,t),Fs(n,t)}else{if(Ps(n,r))return;js.set(r),e.uniformMatrix3fv(this.addr,!1,js),Fs(n,r)}}function Us(e,t){let n=this.cache,r=t.elements;if(r===void 0){if(Ps(n,t))return;e.uniformMatrix4fv(this.addr,!1,t),Fs(n,t)}else{if(Ps(n,r))return;As.set(r),e.uniformMatrix4fv(this.addr,!1,As),Fs(n,r)}}function Ws(e,t){let n=this.cache;n[0]!==t&&(e.uniform1i(this.addr,t),n[0]=t)}function Gs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y)&&(e.uniform2i(this.addr,t.x,t.y),n[0]=t.x,n[1]=t.y);else{if(Ps(n,t))return;e.uniform2iv(this.addr,t),Fs(n,t)}}function Ks(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y||n[2]!==t.z)&&(e.uniform3i(this.addr,t.x,t.y,t.z),n[0]=t.x,n[1]=t.y,n[2]=t.z);else{if(Ps(n,t))return;e.uniform3iv(this.addr,t),Fs(n,t)}}function qs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y||n[2]!==t.z||n[3]!==t.w)&&(e.uniform4i(this.addr,t.x,t.y,t.z,t.w),n[0]=t.x,n[1]=t.y,n[2]=t.z,n[3]=t.w);else{if(Ps(n,t))return;e.uniform4iv(this.addr,t),Fs(n,t)}}function Js(e,t){let n=this.cache;n[0]!==t&&(e.uniform1ui(this.addr,t),n[0]=t)}function Ys(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y)&&(e.uniform2ui(this.addr,t.x,t.y),n[0]=t.x,n[1]=t.y);else{if(Ps(n,t))return;e.uniform2uiv(this.addr,t),Fs(n,t)}}function Xs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y||n[2]!==t.z)&&(e.uniform3ui(this.addr,t.x,t.y,t.z),n[0]=t.x,n[1]=t.y,n[2]=t.z);else{if(Ps(n,t))return;e.uniform3uiv(this.addr,t),Fs(n,t)}}function Zs(e,t){let n=this.cache;if(t.x!==void 0)(n[0]!==t.x||n[1]!==t.y||n[2]!==t.z||n[3]!==t.w)&&(e.uniform4ui(this.addr,t.x,t.y,t.z,t.w),n[0]=t.x,n[1]=t.y,n[2]=t.z,n[3]=t.w);else{if(Ps(n,t))return;e.uniform4uiv(this.addr,t),Fs(n,t)}}function Qs(e,t,n){let r=this.cache,i=n.allocateTextureUnit();r[0]!==i&&(e.uniform1i(this.addr,i),r[0]=i);let a=this.type===e.SAMPLER_2D_SHADOW?ws:Cs;n.setTexture2D(t||a,i)}function $s(e,t,n){let r=this.cache,i=n.allocateTextureUnit();r[0]!==i&&(e.uniform1i(this.addr,i),r[0]=i),n.setTexture3D(t||Es,i)}function ec(e,t,n){let r=this.cache,i=n.allocateTextureUnit();r[0]!==i&&(e.uniform1i(this.addr,i),r[0]=i),n.setTextureCube(t||Ds,i)}function tc(e,t,n){let r=this.cache,i=n.allocateTextureUnit();r[0]!==i&&(e.uniform1i(this.addr,i),r[0]=i),n.setTexture2DArray(t||Ts,i)}function nc(e){switch(e){case 5126:return Ls;case 35664:return Rs;case 35665:return zs;case 35666:return Bs;case 35674:return Vs;case 35675:return Hs;case 35676:return Us;case 5124:case 35670:return Ws;case 35667:case 35671:return Gs;case 35668:case 35672:return Ks;case 35669:case 35673:return qs;case 5125:return Js;case 36294:return Ys;case 36295:return Xs;case 36296:return Zs;case 35678:case 36198:case 36298:case 36306:case 35682:return Qs;case 35679:case 36299:case 36307:return $s;case 35680:case 36300:case 36308:case 36293:return ec;case 36289:case 36303:case 36311:case 36292:return tc}}function rc(e,t){e.uniform1fv(this.addr,t)}function ic(e,t){let n=Ns(t,this.size,2);e.uniform2fv(this.addr,n)}function ac(e,t){let n=Ns(t,this.size,3);e.uniform3fv(this.addr,n)}function oc(e,t){let n=Ns(t,this.size,4);e.uniform4fv(this.addr,n)}function sc(e,t){let n=Ns(t,this.size,4);e.uniformMatrix2fv(this.addr,!1,n)}function cc(e,t){let n=Ns(t,this.size,9);e.uniformMatrix3fv(this.addr,!1,n)}function lc(e,t){let n=Ns(t,this.size,16);e.uniformMatrix4fv(this.addr,!1,n)}function uc(e,t){e.uniform1iv(this.addr,t)}function dc(e,t){e.uniform2iv(this.addr,t)}function fc(e,t){e.uniform3iv(this.addr,t)}function pc(e,t){e.uniform4iv(this.addr,t)}function mc(e,t){e.uniform1uiv(this.addr,t)}function hc(e,t){e.uniform2uiv(this.addr,t)}function gc(e,t){e.uniform3uiv(this.addr,t)}function _c(e,t){e.uniform4uiv(this.addr,t)}function vc(e,t,n){let r=this.cache,i=t.length,a=Is(n,i);Ps(r,a)||(e.uniform1iv(this.addr,a),Fs(r,a));for(let e=0;e!==i;++e)n.setTexture2D(t[e]||Cs,a[e])}function yc(e,t,n){let r=this.cache,i=t.length,a=Is(n,i);Ps(r,a)||(e.uniform1iv(this.addr,a),Fs(r,a));for(let e=0;e!==i;++e)n.setTexture3D(t[e]||Es,a[e])}function bc(e,t,n){let r=this.cache,i=t.length,a=Is(n,i);Ps(r,a)||(e.uniform1iv(this.addr,a),Fs(r,a));for(let e=0;e!==i;++e)n.setTextureCube(t[e]||Ds,a[e])}function xc(e,t,n){let r=this.cache,i=t.length,a=Is(n,i);Ps(r,a)||(e.uniform1iv(this.addr,a),Fs(r,a));for(let e=0;e!==i;++e)n.setTexture2DArray(t[e]||Ts,a[e])}function Sc(e){switch(e){case 5126:return rc;case 35664:return ic;case 35665:return ac;case 35666:return oc;case 35674:return sc;case 35675:return cc;case 35676:return lc;case 5124:case 35670:return uc;case 35667:case 35671:return dc;case 35668:case 35672:return fc;case 35669:case 35673:return pc;case 5125:return mc;case 36294:return hc;case 36295:return gc;case 36296:return _c;case 35678:case 36198:case 36298:case 36306:case 35682:return vc;case 35679:case 36299:case 36307:return yc;case 35680:case 36300:case 36308:case 36293:return bc;case 36289:case 36303:case 36311:case 36292:return xc}}var Cc=class{constructor(e,t,n){this.id=e,this.addr=n,this.cache=[],this.type=t.type,this.setValue=nc(t.type)}},wc=class{constructor(e,t,n){this.id=e,this.addr=n,this.cache=[],this.type=t.type,this.size=t.size,this.setValue=Sc(t.type)}},Tc=class{constructor(e){this.id=e,this.seq=[],this.map={}}setValue(e,t,n){let r=this.seq;for(let i=0,a=r.length;i!==a;++i){let a=r[i];a.setValue(e,t[a.id],n)}}},Ec=/(\w+)(\])?(\[|\.)?/g;function Dc(e,t){e.seq.push(t),e.map[t.id]=t}function Oc(e,t,n){let r=e.name,i=r.length;for(Ec.lastIndex=0;;){let a=Ec.exec(r),o=Ec.lastIndex,s=a[1],c=a[2]===`]`,l=a[3];if(c&&(s|=0),l===void 0||l===`[`&&o+2===i){Dc(n,l===void 0?new Cc(s,e,t):new wc(s,e,t));break}else{let e=n.map[s];e===void 0&&(e=new Tc(s),Dc(n,e)),n=e}}}var kc=class{constructor(e,t){this.seq=[],this.map={};let n=e.getProgramParameter(t,e.ACTIVE_UNIFORMS);for(let r=0;r<n;++r){let n=e.getActiveUniform(t,r);Oc(n,e.getUniformLocation(t,n.name),this)}}setValue(e,t,n,r){let i=this.map[t];i!==void 0&&i.setValue(e,n,r)}setOptional(e,t,n){let r=t[n];r!==void 0&&this.setValue(e,n,r)}static upload(e,t,n,r){for(let i=0,a=t.length;i!==a;++i){let a=t[i],o=n[a.id];o.needsUpdate!==!1&&a.setValue(e,o.value,r)}}static seqWithValue(e,t){let n=[];for(let r=0,i=e.length;r!==i;++r){let i=e[r];i.id in t&&n.push(i)}return n}};function Ac(e,t,n){let r=e.createShader(t);return e.shaderSource(r,n),e.compileShader(r),r}var jc=37297,Mc=0;function Nc(e,t){let n=e.split(`
`),r=[],i=Math.max(t-6,0),a=Math.min(t+6,n.length);for(let e=i;e<a;e++){let i=e+1;r.push(`${i===t?`>`:` `} ${i}: ${n[e]}`)}return r.join(`
`)}function Pc(e){let t=Hr.getPrimaries(Hr.workingColorSpace),n=Hr.getPrimaries(e),r;switch(t===n?r=``:t===`p3`&&n===`rec709`?r=`LinearDisplayP3ToLinearSRGB`:t===`rec709`&&n===`p3`&&(r=`LinearSRGBToLinearDisplayP3`),e){case Jn:case Xn:return[r,`LinearTransferOETF`];case qn:case Yn:return[r,`sRGBTransferOETF`];default:return console.warn(`THREE.WebGLProgram: Unsupported color space:`,e),[r,`LinearTransferOETF`]}}function Fc(e,t,n){let r=e.getShaderParameter(t,e.COMPILE_STATUS),i=e.getShaderInfoLog(t).trim();if(r&&i===``)return``;let a=/ERROR: 0:(\d+)/.exec(i);if(a){let r=parseInt(a[1]);return n.toUpperCase()+`

`+i+`

`+Nc(e.getShaderSource(t),r)}else return i}function Ic(e,t){let n=Pc(t);return`vec4 ${e}( vec4 value ) { return ${n[0]}( ${n[1]}( value ) ); }`}function Lc(e,t){let n;switch(t){case 1:n=`Linear`;break;case 2:n=`Reinhard`;break;case 3:n=`OptimizedCineon`;break;case 4:n=`ACESFilmic`;break;case 6:n=`AgX`;break;case 7:n=`Neutral`;break;case 5:n=`Custom`;break;default:console.warn(`THREE.WebGLProgram: Unsupported toneMapping:`,t),n=`Linear`}return`vec3 `+e+`( vec3 color ) { return `+n+`ToneMapping( color ); }`}function Rc(e){return[e.extensionDerivatives||e.envMapCubeUVHeight||e.bumpMap||e.normalMapTangentSpace||e.clearcoatNormalMap||e.flatShading||e.alphaToCoverage||e.shaderID===`physical`?`#extension GL_OES_standard_derivatives : enable`:``,(e.extensionFragDepth||e.logarithmicDepthBuffer)&&e.rendererExtensionFragDepth?`#extension GL_EXT_frag_depth : enable`:``,e.extensionDrawBuffers&&e.rendererExtensionDrawBuffers?`#extension GL_EXT_draw_buffers : require`:``,(e.extensionShaderTextureLOD||e.envMap||e.transmission)&&e.rendererExtensionShaderTextureLod?`#extension GL_EXT_shader_texture_lod : enable`:``].filter(Hc).join(`
`)}function zc(e){return[e.extensionClipCullDistance?`#extension GL_ANGLE_clip_cull_distance : require`:``,e.extensionMultiDraw?`#extension GL_ANGLE_multi_draw : require`:``].filter(Hc).join(`
`)}function Bc(e){let t=[];for(let n in e){let r=e[n];r!==!1&&t.push(`#define `+n+` `+r)}return t.join(`
`)}function Vc(e,t){let n={},r=e.getProgramParameter(t,e.ACTIVE_ATTRIBUTES);for(let i=0;i<r;i++){let r=e.getActiveAttrib(t,i),a=r.name,o=1;r.type===e.FLOAT_MAT2&&(o=2),r.type===e.FLOAT_MAT3&&(o=3),r.type===e.FLOAT_MAT4&&(o=4),n[a]={type:r.type,location:e.getAttribLocation(t,a),locationSize:o}}return n}function Hc(e){return e!==``}function Uc(e,t){let n=t.numSpotLightShadows+t.numSpotLightMaps-t.numSpotLightShadowsWithMaps;return e.replace(/NUM_DIR_LIGHTS/g,t.numDirLights).replace(/NUM_SPOT_LIGHTS/g,t.numSpotLights).replace(/NUM_SPOT_LIGHT_MAPS/g,t.numSpotLightMaps).replace(/NUM_SPOT_LIGHT_COORDS/g,n).replace(/NUM_RECT_AREA_LIGHTS/g,t.numRectAreaLights).replace(/NUM_POINT_LIGHTS/g,t.numPointLights).replace(/NUM_HEMI_LIGHTS/g,t.numHemiLights).replace(/NUM_DIR_LIGHT_SHADOWS/g,t.numDirLightShadows).replace(/NUM_SPOT_LIGHT_SHADOWS_WITH_MAPS/g,t.numSpotLightShadowsWithMaps).replace(/NUM_SPOT_LIGHT_SHADOWS/g,t.numSpotLightShadows).replace(/NUM_POINT_LIGHT_SHADOWS/g,t.numPointLightShadows)}function Wc(e,t){return e.replace(/NUM_CLIPPING_PLANES/g,t.numClippingPlanes).replace(/UNION_CLIPPING_PLANES/g,t.numClippingPlanes-t.numClipIntersection)}var Gc=/^[ \t]*#include +<([\w\d./]+)>/gm;function Kc(e){return e.replace(Gc,Jc)}var qc=new Map([[`encodings_fragment`,`colorspace_fragment`],[`encodings_pars_fragment`,`colorspace_pars_fragment`],[`output_fragment`,`opaque_fragment`]]);function Jc(e,t){let n=Q[t];if(n===void 0){let e=qc.get(t);if(e!==void 0)n=Q[e],console.warn(`THREE.WebGLRenderer: Shader chunk "%s" has been deprecated. Use "%s" instead.`,t,e);else throw Error(`Can not resolve #include <`+t+`>`)}return Kc(n)}var Yc=/#pragma unroll_loop_start\s+for\s*\(\s*int\s+i\s*=\s*(\d+)\s*;\s*i\s*<\s*(\d+)\s*;\s*i\s*\+\+\s*\)\s*{([\s\S]+?)}\s+#pragma unroll_loop_end/g;function Xc(e){return e.replace(Yc,Zc)}function Zc(e,t,n,r){let i=``;for(let e=parseInt(t);e<parseInt(n);e++)i+=r.replace(/\[\s*i\s*\]/g,`[ `+e+` ]`).replace(/UNROLLED_LOOP_INDEX/g,e);return i}function Qc(e){let t=`precision ${e.precision} float;
	precision ${e.precision} int;
	precision ${e.precision} sampler2D;
	precision ${e.precision} samplerCube;
	`;return e.isWebGL2&&(t+=`precision ${e.precision} sampler3D;
		precision ${e.precision} sampler2DArray;
		precision ${e.precision} sampler2DShadow;
		precision ${e.precision} samplerCubeShadow;
		precision ${e.precision} sampler2DArrayShadow;
		precision ${e.precision} isampler2D;
		precision ${e.precision} isampler3D;
		precision ${e.precision} isamplerCube;
		precision ${e.precision} isampler2DArray;
		precision ${e.precision} usampler2D;
		precision ${e.precision} usampler3D;
		precision ${e.precision} usamplerCube;
		precision ${e.precision} usampler2DArray;
		`),e.precision===`highp`?t+=`
#define HIGH_PRECISION`:e.precision===`mediump`?t+=`
#define MEDIUM_PRECISION`:e.precision===`lowp`&&(t+=`
#define LOW_PRECISION`),t}function $c(e){let t=`SHADOWMAP_TYPE_BASIC`;return e.shadowMapType===1?t=`SHADOWMAP_TYPE_PCF`:e.shadowMapType===2?t=`SHADOWMAP_TYPE_PCF_SOFT`:e.shadowMapType===3&&(t=`SHADOWMAP_TYPE_VSM`),t}function el(e){let t=`ENVMAP_TYPE_CUBE`;if(e.envMap)switch(e.envMapMode){case 301:case 302:t=`ENVMAP_TYPE_CUBE`;break;case 306:t=`ENVMAP_TYPE_CUBE_UV`;break}return t}function tl(e){let t=`ENVMAP_MODE_REFLECTION`;if(e.envMap)switch(e.envMapMode){case 302:t=`ENVMAP_MODE_REFRACTION`;break}return t}function nl(e){let t=`ENVMAP_BLENDING_NONE`;if(e.envMap)switch(e.combine){case 0:t=`ENVMAP_BLENDING_MULTIPLY`;break;case 1:t=`ENVMAP_BLENDING_MIX`;break;case 2:t=`ENVMAP_BLENDING_ADD`;break}return t}function rl(e){let t=e.envMapCubeUVHeight;if(t===null)return null;let n=Math.log2(t)-2,r=1/t;return{texelWidth:1/(3*Math.max(2**n,112)),texelHeight:r,maxMip:n}}function il(e,t,n,r){let i=e.getContext(),a=n.defines,o=n.vertexShader,s=n.fragmentShader,c=$c(n),l=el(n),u=tl(n),d=nl(n),f=rl(n),p=n.isWebGL2?``:Rc(n),m=zc(n),h=Bc(a),g=i.createProgram(),_,v,y=n.glslVersion?`#version `+n.glslVersion+`
`:``;n.isRawShaderMaterial?(_=[`#define SHADER_TYPE `+n.shaderType,`#define SHADER_NAME `+n.shaderName,h].filter(Hc).join(`
`),_.length>0&&(_+=`
`),v=[p,`#define SHADER_TYPE `+n.shaderType,`#define SHADER_NAME `+n.shaderName,h].filter(Hc).join(`
`),v.length>0&&(v+=`
`)):(_=[Qc(n),`#define SHADER_TYPE `+n.shaderType,`#define SHADER_NAME `+n.shaderName,h,n.extensionClipCullDistance?`#define USE_CLIP_DISTANCE`:``,n.batching?`#define USE_BATCHING`:``,n.instancing?`#define USE_INSTANCING`:``,n.instancingColor?`#define USE_INSTANCING_COLOR`:``,n.instancingMorph?`#define USE_INSTANCING_MORPH`:``,n.useFog&&n.fog?`#define USE_FOG`:``,n.useFog&&n.fogExp2?`#define FOG_EXP2`:``,n.map?`#define USE_MAP`:``,n.envMap?`#define USE_ENVMAP`:``,n.envMap?`#define `+u:``,n.lightMap?`#define USE_LIGHTMAP`:``,n.aoMap?`#define USE_AOMAP`:``,n.bumpMap?`#define USE_BUMPMAP`:``,n.normalMap?`#define USE_NORMALMAP`:``,n.normalMapObjectSpace?`#define USE_NORMALMAP_OBJECTSPACE`:``,n.normalMapTangentSpace?`#define USE_NORMALMAP_TANGENTSPACE`:``,n.displacementMap?`#define USE_DISPLACEMENTMAP`:``,n.emissiveMap?`#define USE_EMISSIVEMAP`:``,n.anisotropy?`#define USE_ANISOTROPY`:``,n.anisotropyMap?`#define USE_ANISOTROPYMAP`:``,n.clearcoatMap?`#define USE_CLEARCOATMAP`:``,n.clearcoatRoughnessMap?`#define USE_CLEARCOAT_ROUGHNESSMAP`:``,n.clearcoatNormalMap?`#define USE_CLEARCOAT_NORMALMAP`:``,n.iridescenceMap?`#define USE_IRIDESCENCEMAP`:``,n.iridescenceThicknessMap?`#define USE_IRIDESCENCE_THICKNESSMAP`:``,n.specularMap?`#define USE_SPECULARMAP`:``,n.specularColorMap?`#define USE_SPECULAR_COLORMAP`:``,n.specularIntensityMap?`#define USE_SPECULAR_INTENSITYMAP`:``,n.roughnessMap?`#define USE_ROUGHNESSMAP`:``,n.metalnessMap?`#define USE_METALNESSMAP`:``,n.alphaMap?`#define USE_ALPHAMAP`:``,n.alphaHash?`#define USE_ALPHAHASH`:``,n.transmission?`#define USE_TRANSMISSION`:``,n.transmissionMap?`#define USE_TRANSMISSIONMAP`:``,n.thicknessMap?`#define USE_THICKNESSMAP`:``,n.sheenColorMap?`#define USE_SHEEN_COLORMAP`:``,n.sheenRoughnessMap?`#define USE_SHEEN_ROUGHNESSMAP`:``,n.mapUv?`#define MAP_UV `+n.mapUv:``,n.alphaMapUv?`#define ALPHAMAP_UV `+n.alphaMapUv:``,n.lightMapUv?`#define LIGHTMAP_UV `+n.lightMapUv:``,n.aoMapUv?`#define AOMAP_UV `+n.aoMapUv:``,n.emissiveMapUv?`#define EMISSIVEMAP_UV `+n.emissiveMapUv:``,n.bumpMapUv?`#define BUMPMAP_UV `+n.bumpMapUv:``,n.normalMapUv?`#define NORMALMAP_UV `+n.normalMapUv:``,n.displacementMapUv?`#define DISPLACEMENTMAP_UV `+n.displacementMapUv:``,n.metalnessMapUv?`#define METALNESSMAP_UV `+n.metalnessMapUv:``,n.roughnessMapUv?`#define ROUGHNESSMAP_UV `+n.roughnessMapUv:``,n.anisotropyMapUv?`#define ANISOTROPYMAP_UV `+n.anisotropyMapUv:``,n.clearcoatMapUv?`#define CLEARCOATMAP_UV `+n.clearcoatMapUv:``,n.clearcoatNormalMapUv?`#define CLEARCOAT_NORMALMAP_UV `+n.clearcoatNormalMapUv:``,n.clearcoatRoughnessMapUv?`#define CLEARCOAT_ROUGHNESSMAP_UV `+n.clearcoatRoughnessMapUv:``,n.iridescenceMapUv?`#define IRIDESCENCEMAP_UV `+n.iridescenceMapUv:``,n.iridescenceThicknessMapUv?`#define IRIDESCENCE_THICKNESSMAP_UV `+n.iridescenceThicknessMapUv:``,n.sheenColorMapUv?`#define SHEEN_COLORMAP_UV `+n.sheenColorMapUv:``,n.sheenRoughnessMapUv?`#define SHEEN_ROUGHNESSMAP_UV `+n.sheenRoughnessMapUv:``,n.specularMapUv?`#define SPECULARMAP_UV `+n.specularMapUv:``,n.specularColorMapUv?`#define SPECULAR_COLORMAP_UV `+n.specularColorMapUv:``,n.specularIntensityMapUv?`#define SPECULAR_INTENSITYMAP_UV `+n.specularIntensityMapUv:``,n.transmissionMapUv?`#define TRANSMISSIONMAP_UV `+n.transmissionMapUv:``,n.thicknessMapUv?`#define THICKNESSMAP_UV `+n.thicknessMapUv:``,n.vertexTangents&&n.flatShading===!1?`#define USE_TANGENT`:``,n.vertexColors?`#define USE_COLOR`:``,n.vertexAlphas?`#define USE_COLOR_ALPHA`:``,n.vertexUv1s?`#define USE_UV1`:``,n.vertexUv2s?`#define USE_UV2`:``,n.vertexUv3s?`#define USE_UV3`:``,n.pointsUvs?`#define USE_POINTS_UV`:``,n.flatShading?`#define FLAT_SHADED`:``,n.skinning?`#define USE_SKINNING`:``,n.morphTargets?`#define USE_MORPHTARGETS`:``,n.morphNormals&&n.flatShading===!1?`#define USE_MORPHNORMALS`:``,n.morphColors&&n.isWebGL2?`#define USE_MORPHCOLORS`:``,n.morphTargetsCount>0&&n.isWebGL2?`#define MORPHTARGETS_TEXTURE`:``,n.morphTargetsCount>0&&n.isWebGL2?`#define MORPHTARGETS_TEXTURE_STRIDE `+n.morphTextureStride:``,n.morphTargetsCount>0&&n.isWebGL2?`#define MORPHTARGETS_COUNT `+n.morphTargetsCount:``,n.doubleSided?`#define DOUBLE_SIDED`:``,n.flipSided?`#define FLIP_SIDED`:``,n.shadowMapEnabled?`#define USE_SHADOWMAP`:``,n.shadowMapEnabled?`#define `+c:``,n.sizeAttenuation?`#define USE_SIZEATTENUATION`:``,n.numLightProbes>0?`#define USE_LIGHT_PROBES`:``,n.useLegacyLights?`#define LEGACY_LIGHTS`:``,n.logarithmicDepthBuffer?`#define USE_LOGDEPTHBUF`:``,n.logarithmicDepthBuffer&&n.rendererExtensionFragDepth?`#define USE_LOGDEPTHBUF_EXT`:``,`uniform mat4 modelMatrix;`,`uniform mat4 modelViewMatrix;`,`uniform mat4 projectionMatrix;`,`uniform mat4 viewMatrix;`,`uniform mat3 normalMatrix;`,`uniform vec3 cameraPosition;`,`uniform bool isOrthographic;`,`#ifdef USE_INSTANCING`,`	attribute mat4 instanceMatrix;`,`#endif`,`#ifdef USE_INSTANCING_COLOR`,`	attribute vec3 instanceColor;`,`#endif`,`#ifdef USE_INSTANCING_MORPH`,`	uniform sampler2D morphTexture;`,`#endif`,`attribute vec3 position;`,`attribute vec3 normal;`,`attribute vec2 uv;`,`#ifdef USE_UV1`,`	attribute vec2 uv1;`,`#endif`,`#ifdef USE_UV2`,`	attribute vec2 uv2;`,`#endif`,`#ifdef USE_UV3`,`	attribute vec2 uv3;`,`#endif`,`#ifdef USE_TANGENT`,`	attribute vec4 tangent;`,`#endif`,`#if defined( USE_COLOR_ALPHA )`,`	attribute vec4 color;`,`#elif defined( USE_COLOR )`,`	attribute vec3 color;`,`#endif`,`#if ( defined( USE_MORPHTARGETS ) && ! defined( MORPHTARGETS_TEXTURE ) )`,`	attribute vec3 morphTarget0;`,`	attribute vec3 morphTarget1;`,`	attribute vec3 morphTarget2;`,`	attribute vec3 morphTarget3;`,`	#ifdef USE_MORPHNORMALS`,`		attribute vec3 morphNormal0;`,`		attribute vec3 morphNormal1;`,`		attribute vec3 morphNormal2;`,`		attribute vec3 morphNormal3;`,`	#else`,`		attribute vec3 morphTarget4;`,`		attribute vec3 morphTarget5;`,`		attribute vec3 morphTarget6;`,`		attribute vec3 morphTarget7;`,`	#endif`,`#endif`,`#ifdef USE_SKINNING`,`	attribute vec4 skinIndex;`,`	attribute vec4 skinWeight;`,`#endif`,`
`].filter(Hc).join(`
`),v=[p,Qc(n),`#define SHADER_TYPE `+n.shaderType,`#define SHADER_NAME `+n.shaderName,h,n.useFog&&n.fog?`#define USE_FOG`:``,n.useFog&&n.fogExp2?`#define FOG_EXP2`:``,n.alphaToCoverage?`#define ALPHA_TO_COVERAGE`:``,n.map?`#define USE_MAP`:``,n.matcap?`#define USE_MATCAP`:``,n.envMap?`#define USE_ENVMAP`:``,n.envMap?`#define `+l:``,n.envMap?`#define `+u:``,n.envMap?`#define `+d:``,f?`#define CUBEUV_TEXEL_WIDTH `+f.texelWidth:``,f?`#define CUBEUV_TEXEL_HEIGHT `+f.texelHeight:``,f?`#define CUBEUV_MAX_MIP `+f.maxMip+`.0`:``,n.lightMap?`#define USE_LIGHTMAP`:``,n.aoMap?`#define USE_AOMAP`:``,n.bumpMap?`#define USE_BUMPMAP`:``,n.normalMap?`#define USE_NORMALMAP`:``,n.normalMapObjectSpace?`#define USE_NORMALMAP_OBJECTSPACE`:``,n.normalMapTangentSpace?`#define USE_NORMALMAP_TANGENTSPACE`:``,n.emissiveMap?`#define USE_EMISSIVEMAP`:``,n.anisotropy?`#define USE_ANISOTROPY`:``,n.anisotropyMap?`#define USE_ANISOTROPYMAP`:``,n.clearcoat?`#define USE_CLEARCOAT`:``,n.clearcoatMap?`#define USE_CLEARCOATMAP`:``,n.clearcoatRoughnessMap?`#define USE_CLEARCOAT_ROUGHNESSMAP`:``,n.clearcoatNormalMap?`#define USE_CLEARCOAT_NORMALMAP`:``,n.iridescence?`#define USE_IRIDESCENCE`:``,n.iridescenceMap?`#define USE_IRIDESCENCEMAP`:``,n.iridescenceThicknessMap?`#define USE_IRIDESCENCE_THICKNESSMAP`:``,n.specularMap?`#define USE_SPECULARMAP`:``,n.specularColorMap?`#define USE_SPECULAR_COLORMAP`:``,n.specularIntensityMap?`#define USE_SPECULAR_INTENSITYMAP`:``,n.roughnessMap?`#define USE_ROUGHNESSMAP`:``,n.metalnessMap?`#define USE_METALNESSMAP`:``,n.alphaMap?`#define USE_ALPHAMAP`:``,n.alphaTest?`#define USE_ALPHATEST`:``,n.alphaHash?`#define USE_ALPHAHASH`:``,n.sheen?`#define USE_SHEEN`:``,n.sheenColorMap?`#define USE_SHEEN_COLORMAP`:``,n.sheenRoughnessMap?`#define USE_SHEEN_ROUGHNESSMAP`:``,n.transmission?`#define USE_TRANSMISSION`:``,n.transmissionMap?`#define USE_TRANSMISSIONMAP`:``,n.thicknessMap?`#define USE_THICKNESSMAP`:``,n.vertexTangents&&n.flatShading===!1?`#define USE_TANGENT`:``,n.vertexColors||n.instancingColor?`#define USE_COLOR`:``,n.vertexAlphas?`#define USE_COLOR_ALPHA`:``,n.vertexUv1s?`#define USE_UV1`:``,n.vertexUv2s?`#define USE_UV2`:``,n.vertexUv3s?`#define USE_UV3`:``,n.pointsUvs?`#define USE_POINTS_UV`:``,n.gradientMap?`#define USE_GRADIENTMAP`:``,n.flatShading?`#define FLAT_SHADED`:``,n.doubleSided?`#define DOUBLE_SIDED`:``,n.flipSided?`#define FLIP_SIDED`:``,n.shadowMapEnabled?`#define USE_SHADOWMAP`:``,n.shadowMapEnabled?`#define `+c:``,n.premultipliedAlpha?`#define PREMULTIPLIED_ALPHA`:``,n.numLightProbes>0?`#define USE_LIGHT_PROBES`:``,n.useLegacyLights?`#define LEGACY_LIGHTS`:``,n.decodeVideoTexture?`#define DECODE_VIDEO_TEXTURE`:``,n.logarithmicDepthBuffer?`#define USE_LOGDEPTHBUF`:``,n.logarithmicDepthBuffer&&n.rendererExtensionFragDepth?`#define USE_LOGDEPTHBUF_EXT`:``,`uniform mat4 viewMatrix;`,`uniform vec3 cameraPosition;`,`uniform bool isOrthographic;`,n.toneMapping===0?``:`#define TONE_MAPPING`,n.toneMapping===0?``:Q.tonemapping_pars_fragment,n.toneMapping===0?``:Lc(`toneMapping`,n.toneMapping),n.dithering?`#define DITHERING`:``,n.opaque?`#define OPAQUE`:``,Q.colorspace_pars_fragment,Ic(`linearToOutputTexel`,n.outputColorSpace),n.useDepthPacking?`#define DEPTH_PACKING `+n.depthPacking:``,`
`].filter(Hc).join(`
`)),o=Kc(o),o=Uc(o,n),o=Wc(o,n),s=Kc(s),s=Uc(s,n),s=Wc(s,n),o=Xc(o),s=Xc(s),n.isWebGL2&&n.isRawShaderMaterial!==!0&&(y=`#version 300 es
`,_=[m,`precision mediump sampler2DArray;`,`#define attribute in`,`#define varying out`,`#define texture2D texture`].join(`
`)+`
`+_,v=[`precision mediump sampler2DArray;`,`#define varying in`,n.glslVersion===`300 es`?``:`layout(location = 0) out highp vec4 pc_fragColor;`,n.glslVersion===`300 es`?``:`#define gl_FragColor pc_fragColor`,`#define gl_FragDepthEXT gl_FragDepth`,`#define texture2D texture`,`#define textureCube texture`,`#define texture2DProj textureProj`,`#define texture2DLodEXT textureLod`,`#define texture2DProjLodEXT textureProjLod`,`#define textureCubeLodEXT textureLod`,`#define texture2DGradEXT textureGrad`,`#define texture2DProjGradEXT textureProjGrad`,`#define textureCubeGradEXT textureGrad`].join(`
`)+`
`+v);let b=y+_+o,x=y+v+s,S=Ac(i,i.VERTEX_SHADER,b),C=Ac(i,i.FRAGMENT_SHADER,x);i.attachShader(g,S),i.attachShader(g,C),n.index0AttributeName===void 0?n.morphTargets===!0&&i.bindAttribLocation(g,0,`position`):i.bindAttribLocation(g,0,n.index0AttributeName),i.linkProgram(g);function w(t){if(e.debug.checkShaderErrors){let n=i.getProgramInfoLog(g).trim(),r=i.getShaderInfoLog(S).trim(),a=i.getShaderInfoLog(C).trim(),o=!0,s=!0;if(i.getProgramParameter(g,i.LINK_STATUS)===!1)if(o=!1,typeof e.debug.onShaderError==`function`)e.debug.onShaderError(i,g,S,C);else{let e=Fc(i,S,`vertex`),r=Fc(i,C,`fragment`);console.error(`THREE.WebGLProgram: Shader Error `+i.getError()+` - VALIDATE_STATUS `+i.getProgramParameter(g,i.VALIDATE_STATUS)+`

Material Name: `+t.name+`
Material Type: `+t.type+`

Program Info Log: `+n+`
`+e+`
`+r)}else n===``?(r===``||a===``)&&(s=!1):console.warn(`THREE.WebGLProgram: Program Info Log:`,n);s&&(t.diagnostics={runnable:o,programLog:n,vertexShader:{log:r,prefix:_},fragmentShader:{log:a,prefix:v}})}i.deleteShader(S),i.deleteShader(C),T=new kc(i,g),E=Vc(i,g)}let T;this.getUniforms=function(){return T===void 0&&w(this),T};let E;this.getAttributes=function(){return E===void 0&&w(this),E};let D=n.rendererExtensionParallelShaderCompile===!1;return this.isReady=function(){return D===!1&&(D=i.getProgramParameter(g,jc)),D},this.destroy=function(){r.releaseStatesOfProgram(this),i.deleteProgram(g),this.program=void 0},this.type=n.shaderType,this.name=n.shaderName,this.id=Mc++,this.cacheKey=t,this.usedTimes=1,this.program=g,this.vertexShader=S,this.fragmentShader=C,this}var al=0,ol=class{constructor(){this.shaderCache=new Map,this.materialCache=new Map}update(e){let t=e.vertexShader,n=e.fragmentShader,r=this._getShaderStage(t),i=this._getShaderStage(n),a=this._getShaderCacheForMaterial(e);return a.has(r)===!1&&(a.add(r),r.usedTimes++),a.has(i)===!1&&(a.add(i),i.usedTimes++),this}remove(e){let t=this.materialCache.get(e);for(let e of t)e.usedTimes--,e.usedTimes===0&&this.shaderCache.delete(e.code);return this.materialCache.delete(e),this}getVertexShaderID(e){return this._getShaderStage(e.vertexShader).id}getFragmentShaderID(e){return this._getShaderStage(e.fragmentShader).id}dispose(){this.shaderCache.clear(),this.materialCache.clear()}_getShaderCacheForMaterial(e){let t=this.materialCache,n=t.get(e);return n===void 0&&(n=new Set,t.set(e,n)),n}_getShaderStage(e){let t=this.shaderCache,n=t.get(e);return n===void 0&&(n=new sl(e),t.set(e,n)),n}},sl=class{constructor(e){this.id=al++,this.code=e,this.usedTimes=0}};function cl(e,t,n,r,i,a,o){let s=new Wi,c=new ol,l=new Set,u=[],d=i.isWebGL2,f=i.logarithmicDepthBuffer,p=i.vertexTextures,m=i.precision,h={MeshDepthMaterial:`depth`,MeshDistanceMaterial:`distanceRGBA`,MeshNormalMaterial:`normal`,MeshBasicMaterial:`basic`,MeshLambertMaterial:`lambert`,MeshPhongMaterial:`phong`,MeshToonMaterial:`toon`,MeshStandardMaterial:`physical`,MeshPhysicalMaterial:`physical`,MeshMatcapMaterial:`matcap`,LineBasicMaterial:`basic`,LineDashedMaterial:`dashed`,PointsMaterial:`points`,ShadowMaterial:`shadow`,SpriteMaterial:`sprite`};function g(e){return l.add(e),e===0?`uv`:`uv${e}`}function _(a,s,u,_,v){let y=_.fog,b=v.geometry,x=a.isMeshStandardMaterial?_.environment:null,S=(a.isMeshStandardMaterial?n:t).get(a.envMap||x),C=S&&S.mapping===306?S.image.height:null,w=h[a.type];a.precision!==null&&(m=i.getMaxPrecision(a.precision),m!==a.precision&&console.warn(`THREE.WebGLProgram.getParameters:`,a.precision,`not supported, using`,m,`instead.`));let T=b.morphAttributes.position||b.morphAttributes.normal||b.morphAttributes.color,E=T===void 0?0:T.length,D=0;b.morphAttributes.position!==void 0&&(D=1),b.morphAttributes.normal!==void 0&&(D=2),b.morphAttributes.color!==void 0&&(D=3);let O,k,A,j;if(w){let e=Lo[w];O=e.vertexShader,k=e.fragmentShader}else O=a.vertexShader,k=a.fragmentShader,c.update(a),A=c.getVertexShaderID(a),j=c.getFragmentShaderID(a);let M=e.getRenderTarget(),N=v.isInstancedMesh===!0,P=v.isBatchedMesh===!0,F=!!a.map,ee=!!a.matcap,I=!!S,te=!!a.aoMap,ne=!!a.lightMap,re=!!a.bumpMap,ie=!!a.normalMap,ae=!!a.displacementMap,oe=!!a.emissiveMap,L=!!a.metalnessMap,se=!!a.roughnessMap,ce=a.anisotropy>0,R=a.clearcoat>0,le=a.iridescence>0,z=a.sheen>0,B=a.transmission>0,V=ce&&!!a.anisotropyMap,H=R&&!!a.clearcoatMap,U=R&&!!a.clearcoatNormalMap,W=R&&!!a.clearcoatRoughnessMap,ue=le&&!!a.iridescenceMap,de=le&&!!a.iridescenceThicknessMap,fe=z&&!!a.sheenColorMap,pe=z&&!!a.sheenRoughnessMap,me=!!a.specularMap,he=!!a.specularColorMap,ge=!!a.specularIntensityMap,_e=B&&!!a.transmissionMap,ve=B&&!!a.thicknessMap,ye=!!a.gradientMap,be=!!a.alphaMap,xe=a.alphaTest>0,Se=!!a.alphaHash,G=!!a.extensions,Ce=0;a.toneMapped&&(M===null||M.isXRRenderTarget===!0)&&(Ce=e.toneMapping);let we={isWebGL2:d,shaderID:w,shaderType:a.type,shaderName:a.name,vertexShader:O,fragmentShader:k,defines:a.defines,customVertexShaderID:A,customFragmentShaderID:j,isRawShaderMaterial:a.isRawShaderMaterial===!0,glslVersion:a.glslVersion,precision:m,batching:P,instancing:N,instancingColor:N&&v.instanceColor!==null,instancingMorph:N&&v.morphTexture!==null,supportsVertexTextures:p,outputColorSpace:M===null?e.outputColorSpace:M.isXRRenderTarget===!0?M.texture.colorSpace:Jn,alphaToCoverage:!!a.alphaToCoverage,map:F,matcap:ee,envMap:I,envMapMode:I&&S.mapping,envMapCubeUVHeight:C,aoMap:te,lightMap:ne,bumpMap:re,normalMap:ie,displacementMap:p&&ae,emissiveMap:oe,normalMapObjectSpace:ie&&a.normalMapType===1,normalMapTangentSpace:ie&&a.normalMapType===0,metalnessMap:L,roughnessMap:se,anisotropy:ce,anisotropyMap:V,clearcoat:R,clearcoatMap:H,clearcoatNormalMap:U,clearcoatRoughnessMap:W,iridescence:le,iridescenceMap:ue,iridescenceThicknessMap:de,sheen:z,sheenColorMap:fe,sheenRoughnessMap:pe,specularMap:me,specularColorMap:he,specularIntensityMap:ge,transmission:B,transmissionMap:_e,thicknessMap:ve,gradientMap:ye,opaque:a.transparent===!1&&a.blending===1&&a.alphaToCoverage===!1,alphaMap:be,alphaTest:xe,alphaHash:Se,combine:a.combine,mapUv:F&&g(a.map.channel),aoMapUv:te&&g(a.aoMap.channel),lightMapUv:ne&&g(a.lightMap.channel),bumpMapUv:re&&g(a.bumpMap.channel),normalMapUv:ie&&g(a.normalMap.channel),displacementMapUv:ae&&g(a.displacementMap.channel),emissiveMapUv:oe&&g(a.emissiveMap.channel),metalnessMapUv:L&&g(a.metalnessMap.channel),roughnessMapUv:se&&g(a.roughnessMap.channel),anisotropyMapUv:V&&g(a.anisotropyMap.channel),clearcoatMapUv:H&&g(a.clearcoatMap.channel),clearcoatNormalMapUv:U&&g(a.clearcoatNormalMap.channel),clearcoatRoughnessMapUv:W&&g(a.clearcoatRoughnessMap.channel),iridescenceMapUv:ue&&g(a.iridescenceMap.channel),iridescenceThicknessMapUv:de&&g(a.iridescenceThicknessMap.channel),sheenColorMapUv:fe&&g(a.sheenColorMap.channel),sheenRoughnessMapUv:pe&&g(a.sheenRoughnessMap.channel),specularMapUv:me&&g(a.specularMap.channel),specularColorMapUv:he&&g(a.specularColorMap.channel),specularIntensityMapUv:ge&&g(a.specularIntensityMap.channel),transmissionMapUv:_e&&g(a.transmissionMap.channel),thicknessMapUv:ve&&g(a.thicknessMap.channel),alphaMapUv:be&&g(a.alphaMap.channel),vertexTangents:!!b.attributes.tangent&&(ie||ce),vertexColors:a.vertexColors,vertexAlphas:a.vertexColors===!0&&!!b.attributes.color&&b.attributes.color.itemSize===4,pointsUvs:v.isPoints===!0&&!!b.attributes.uv&&(F||be),fog:!!y,useFog:a.fog===!0,fogExp2:!!y&&y.isFogExp2,flatShading:a.flatShading===!0,sizeAttenuation:a.sizeAttenuation===!0,logarithmicDepthBuffer:f,skinning:v.isSkinnedMesh===!0,morphTargets:b.morphAttributes.position!==void 0,morphNormals:b.morphAttributes.normal!==void 0,morphColors:b.morphAttributes.color!==void 0,morphTargetsCount:E,morphTextureStride:D,numDirLights:s.directional.length,numPointLights:s.point.length,numSpotLights:s.spot.length,numSpotLightMaps:s.spotLightMap.length,numRectAreaLights:s.rectArea.length,numHemiLights:s.hemi.length,numDirLightShadows:s.directionalShadowMap.length,numPointLightShadows:s.pointShadowMap.length,numSpotLightShadows:s.spotShadowMap.length,numSpotLightShadowsWithMaps:s.numSpotLightShadowsWithMaps,numLightProbes:s.numLightProbes,numClippingPlanes:o.numPlanes,numClipIntersection:o.numIntersection,dithering:a.dithering,shadowMapEnabled:e.shadowMap.enabled&&u.length>0,shadowMapType:e.shadowMap.type,toneMapping:Ce,useLegacyLights:e._useLegacyLights,decodeVideoTexture:F&&a.map.isVideoTexture===!0&&Hr.getTransfer(a.map.colorSpace)===`srgb`,premultipliedAlpha:a.premultipliedAlpha,doubleSided:a.side===2,flipSided:a.side===1,useDepthPacking:a.depthPacking>=0,depthPacking:a.depthPacking||0,index0AttributeName:a.index0AttributeName,extensionDerivatives:G&&a.extensions.derivatives===!0,extensionFragDepth:G&&a.extensions.fragDepth===!0,extensionDrawBuffers:G&&a.extensions.drawBuffers===!0,extensionShaderTextureLOD:G&&a.extensions.shaderTextureLOD===!0,extensionClipCullDistance:G&&a.extensions.clipCullDistance===!0&&r.has(`WEBGL_clip_cull_distance`),extensionMultiDraw:G&&a.extensions.multiDraw===!0&&r.has(`WEBGL_multi_draw`),rendererExtensionFragDepth:d||r.has(`EXT_frag_depth`),rendererExtensionDrawBuffers:d||r.has(`WEBGL_draw_buffers`),rendererExtensionShaderTextureLod:d||r.has(`EXT_shader_texture_lod`),rendererExtensionParallelShaderCompile:r.has(`KHR_parallel_shader_compile`),customProgramCacheKey:a.customProgramCacheKey()};return we.vertexUv1s=l.has(1),we.vertexUv2s=l.has(2),we.vertexUv3s=l.has(3),l.clear(),we}function v(t){let n=[];if(t.shaderID?n.push(t.shaderID):(n.push(t.customVertexShaderID),n.push(t.customFragmentShaderID)),t.defines!==void 0)for(let e in t.defines)n.push(e),n.push(t.defines[e]);return t.isRawShaderMaterial===!1&&(y(n,t),b(n,t),n.push(e.outputColorSpace)),n.push(t.customProgramCacheKey),n.join()}function y(e,t){e.push(t.precision),e.push(t.outputColorSpace),e.push(t.envMapMode),e.push(t.envMapCubeUVHeight),e.push(t.mapUv),e.push(t.alphaMapUv),e.push(t.lightMapUv),e.push(t.aoMapUv),e.push(t.bumpMapUv),e.push(t.normalMapUv),e.push(t.displacementMapUv),e.push(t.emissiveMapUv),e.push(t.metalnessMapUv),e.push(t.roughnessMapUv),e.push(t.anisotropyMapUv),e.push(t.clearcoatMapUv),e.push(t.clearcoatNormalMapUv),e.push(t.clearcoatRoughnessMapUv),e.push(t.iridescenceMapUv),e.push(t.iridescenceThicknessMapUv),e.push(t.sheenColorMapUv),e.push(t.sheenRoughnessMapUv),e.push(t.specularMapUv),e.push(t.specularColorMapUv),e.push(t.specularIntensityMapUv),e.push(t.transmissionMapUv),e.push(t.thicknessMapUv),e.push(t.combine),e.push(t.fogExp2),e.push(t.sizeAttenuation),e.push(t.morphTargetsCount),e.push(t.morphAttributeCount),e.push(t.numDirLights),e.push(t.numPointLights),e.push(t.numSpotLights),e.push(t.numSpotLightMaps),e.push(t.numHemiLights),e.push(t.numRectAreaLights),e.push(t.numDirLightShadows),e.push(t.numPointLightShadows),e.push(t.numSpotLightShadows),e.push(t.numSpotLightShadowsWithMaps),e.push(t.numLightProbes),e.push(t.shadowMapType),e.push(t.toneMapping),e.push(t.numClippingPlanes),e.push(t.numClipIntersection),e.push(t.depthPacking)}function b(e,t){s.disableAll(),t.isWebGL2&&s.enable(0),t.supportsVertexTextures&&s.enable(1),t.instancing&&s.enable(2),t.instancingColor&&s.enable(3),t.instancingMorph&&s.enable(4),t.matcap&&s.enable(5),t.envMap&&s.enable(6),t.normalMapObjectSpace&&s.enable(7),t.normalMapTangentSpace&&s.enable(8),t.clearcoat&&s.enable(9),t.iridescence&&s.enable(10),t.alphaTest&&s.enable(11),t.vertexColors&&s.enable(12),t.vertexAlphas&&s.enable(13),t.vertexUv1s&&s.enable(14),t.vertexUv2s&&s.enable(15),t.vertexUv3s&&s.enable(16),t.vertexTangents&&s.enable(17),t.anisotropy&&s.enable(18),t.alphaHash&&s.enable(19),t.batching&&s.enable(20),e.push(s.mask),s.disableAll(),t.fog&&s.enable(0),t.useFog&&s.enable(1),t.flatShading&&s.enable(2),t.logarithmicDepthBuffer&&s.enable(3),t.skinning&&s.enable(4),t.morphTargets&&s.enable(5),t.morphNormals&&s.enable(6),t.morphColors&&s.enable(7),t.premultipliedAlpha&&s.enable(8),t.shadowMapEnabled&&s.enable(9),t.useLegacyLights&&s.enable(10),t.doubleSided&&s.enable(11),t.flipSided&&s.enable(12),t.useDepthPacking&&s.enable(13),t.dithering&&s.enable(14),t.transmission&&s.enable(15),t.sheen&&s.enable(16),t.opaque&&s.enable(17),t.pointsUvs&&s.enable(18),t.decodeVideoTexture&&s.enable(19),t.alphaToCoverage&&s.enable(20),e.push(s.mask)}function x(e){let t=h[e.type],n;if(t){let e=Lo[t];n=po.clone(e.uniforms)}else n=e.uniforms;return n}function S(t,n){let r;for(let e=0,t=u.length;e<t;e++){let t=u[e];if(t.cacheKey===n){r=t,++r.usedTimes;break}}return r===void 0&&(r=new il(e,n,t,a),u.push(r)),r}function C(e){if(--e.usedTimes===0){let t=u.indexOf(e);u[t]=u[u.length-1],u.pop(),e.destroy()}}function w(e){c.remove(e)}function T(){c.dispose()}return{getParameters:_,getProgramCacheKey:v,getUniforms:x,acquireProgram:S,releaseProgram:C,releaseShaderCache:w,programs:u,dispose:T}}function ll(){let e=new WeakMap;function t(t){let n=e.get(t);return n===void 0&&(n={},e.set(t,n)),n}function n(t){e.delete(t)}function r(t,n,r){e.get(t)[n]=r}function i(){e=new WeakMap}return{get:t,remove:n,update:r,dispose:i}}function ul(e,t){return e.groupOrder===t.groupOrder?e.renderOrder===t.renderOrder?e.material.id===t.material.id?e.z===t.z?e.id-t.id:e.z-t.z:e.material.id-t.material.id:e.renderOrder-t.renderOrder:e.groupOrder-t.groupOrder}function dl(e,t){return e.groupOrder===t.groupOrder?e.renderOrder===t.renderOrder?e.z===t.z?e.id-t.id:t.z-e.z:e.renderOrder-t.renderOrder:e.groupOrder-t.groupOrder}function fl(){let e=[],t=0,n=[],r=[],i=[];function a(){t=0,n.length=0,r.length=0,i.length=0}function o(n,r,i,a,o,s){let c=e[t];return c===void 0?(c={id:n.id,object:n,geometry:r,material:i,groupOrder:a,renderOrder:n.renderOrder,z:o,group:s},e[t]=c):(c.id=n.id,c.object=n,c.geometry=r,c.material=i,c.groupOrder=a,c.renderOrder=n.renderOrder,c.z=o,c.group=s),t++,c}function s(e,t,a,s,c,l){let u=o(e,t,a,s,c,l);a.transmission>0?r.push(u):a.transparent===!0?i.push(u):n.push(u)}function c(e,t,a,s,c,l){let u=o(e,t,a,s,c,l);a.transmission>0?r.unshift(u):a.transparent===!0?i.unshift(u):n.unshift(u)}function l(e,t){n.length>1&&n.sort(e||ul),r.length>1&&r.sort(t||dl),i.length>1&&i.sort(t||dl)}function u(){for(let n=t,r=e.length;n<r;n++){let t=e[n];if(t.id===null)break;t.id=null,t.object=null,t.geometry=null,t.material=null,t.group=null}}return{opaque:n,transmissive:r,transparent:i,init:a,push:s,unshift:c,finish:u,sort:l}}function pl(){let e=new WeakMap;function t(t,n){let r=e.get(t),i;return r===void 0?(i=new fl,e.set(t,[i])):n>=r.length?(i=new fl,r.push(i)):i=r[n],i}function n(){e=new WeakMap}return{get:t,dispose:n}}function ml(){let e={};return{get:function(t){if(e[t.id]!==void 0)return e[t.id];let n;switch(t.type){case`DirectionalLight`:n={direction:new Z,color:new Sa};break;case`SpotLight`:n={position:new Z,direction:new Z,color:new Sa,distance:0,coneCos:0,penumbraCos:0,decay:0};break;case`PointLight`:n={position:new Z,color:new Sa,distance:0,decay:0};break;case`HemisphereLight`:n={direction:new Z,skyColor:new Sa,groundColor:new Sa};break;case`RectAreaLight`:n={color:new Sa,position:new Z,halfWidth:new Z,halfHeight:new Z};break}return e[t.id]=n,n}}}function hl(){let e={};return{get:function(t){if(e[t.id]!==void 0)return e[t.id];let n;switch(t.type){case`DirectionalLight`:n={shadowBias:0,shadowNormalBias:0,shadowRadius:1,shadowMapSize:new Y};break;case`SpotLight`:n={shadowBias:0,shadowNormalBias:0,shadowRadius:1,shadowMapSize:new Y};break;case`PointLight`:n={shadowBias:0,shadowNormalBias:0,shadowRadius:1,shadowMapSize:new Y,shadowCameraNear:1,shadowCameraFar:1e3};break}return e[t.id]=n,n}}}var gl=0;function _l(e,t){return(t.castShadow?2:0)-(e.castShadow?2:0)+(t.map?1:0)-(e.map?1:0)}function vl(e,t){let n=new ml,r=hl(),i={version:0,hash:{directionalLength:-1,pointLength:-1,spotLength:-1,rectAreaLength:-1,hemiLength:-1,numDirectionalShadows:-1,numPointShadows:-1,numSpotShadows:-1,numSpotMaps:-1,numLightProbes:-1},ambient:[0,0,0],probe:[],directional:[],directionalShadow:[],directionalShadowMap:[],directionalShadowMatrix:[],spot:[],spotLightMap:[],spotShadow:[],spotShadowMap:[],spotLightMatrix:[],rectArea:[],rectAreaLTC1:null,rectAreaLTC2:null,point:[],pointShadow:[],pointShadowMap:[],pointShadowMatrix:[],hemi:[],numSpotLightShadowsWithMaps:0,numLightProbes:0};for(let e=0;e<9;e++)i.probe.push(new Z);let a=new Z,o=new Ni,s=new Ni;function c(a,o){let s=0,c=0,l=0;for(let e=0;e<9;e++)i.probe[e].set(0,0,0);let u=0,d=0,f=0,p=0,m=0,h=0,g=0,_=0,v=0,y=0,b=0;a.sort(_l);let x=o===!0?Math.PI:1;for(let e=0,t=a.length;e<t;e++){let t=a[e],o=t.color,S=t.intensity,C=t.distance,w=t.shadow&&t.shadow.map?t.shadow.map.texture:null;if(t.isAmbientLight)s+=o.r*S*x,c+=o.g*S*x,l+=o.b*S*x;else if(t.isLightProbe){for(let e=0;e<9;e++)i.probe[e].addScaledVector(t.sh.coefficients[e],S);b++}else if(t.isDirectionalLight){let e=n.get(t);if(e.color.copy(t.color).multiplyScalar(t.intensity*x),t.castShadow){let e=t.shadow,n=r.get(t);n.shadowBias=e.bias,n.shadowNormalBias=e.normalBias,n.shadowRadius=e.radius,n.shadowMapSize=e.mapSize,i.directionalShadow[u]=n,i.directionalShadowMap[u]=w,i.directionalShadowMatrix[u]=t.shadow.matrix,h++}i.directional[u]=e,u++}else if(t.isSpotLight){let e=n.get(t);e.position.setFromMatrixPosition(t.matrixWorld),e.color.copy(o).multiplyScalar(S*x),e.distance=C,e.coneCos=Math.cos(t.angle),e.penumbraCos=Math.cos(t.angle*(1-t.penumbra)),e.decay=t.decay,i.spot[f]=e;let a=t.shadow;if(t.map&&(i.spotLightMap[v]=t.map,v++,a.updateMatrices(t),t.castShadow&&y++),i.spotLightMatrix[f]=a.matrix,t.castShadow){let e=r.get(t);e.shadowBias=a.bias,e.shadowNormalBias=a.normalBias,e.shadowRadius=a.radius,e.shadowMapSize=a.mapSize,i.spotShadow[f]=e,i.spotShadowMap[f]=w,_++}f++}else if(t.isRectAreaLight){let e=n.get(t);e.color.copy(o).multiplyScalar(S),e.halfWidth.set(t.width*.5,0,0),e.halfHeight.set(0,t.height*.5,0),i.rectArea[p]=e,p++}else if(t.isPointLight){let e=n.get(t);if(e.color.copy(t.color).multiplyScalar(t.intensity*x),e.distance=t.distance,e.decay=t.decay,t.castShadow){let e=t.shadow,n=r.get(t);n.shadowBias=e.bias,n.shadowNormalBias=e.normalBias,n.shadowRadius=e.radius,n.shadowMapSize=e.mapSize,n.shadowCameraNear=e.camera.near,n.shadowCameraFar=e.camera.far,i.pointShadow[d]=n,i.pointShadowMap[d]=w,i.pointShadowMatrix[d]=t.shadow.matrix,g++}i.point[d]=e,d++}else if(t.isHemisphereLight){let e=n.get(t);e.skyColor.copy(t.color).multiplyScalar(S*x),e.groundColor.copy(t.groundColor).multiplyScalar(S*x),i.hemi[m]=e,m++}}p>0&&(t.isWebGL2?e.has(`OES_texture_float_linear`)===!0?(i.rectAreaLTC1=$.LTC_FLOAT_1,i.rectAreaLTC2=$.LTC_FLOAT_2):(i.rectAreaLTC1=$.LTC_HALF_1,i.rectAreaLTC2=$.LTC_HALF_2):e.has(`OES_texture_float_linear`)===!0?(i.rectAreaLTC1=$.LTC_FLOAT_1,i.rectAreaLTC2=$.LTC_FLOAT_2):e.has(`OES_texture_half_float_linear`)===!0?(i.rectAreaLTC1=$.LTC_HALF_1,i.rectAreaLTC2=$.LTC_HALF_2):console.error(`THREE.WebGLRenderer: Unable to use RectAreaLight. Missing WebGL extensions.`)),i.ambient[0]=s,i.ambient[1]=c,i.ambient[2]=l;let S=i.hash;(S.directionalLength!==u||S.pointLength!==d||S.spotLength!==f||S.rectAreaLength!==p||S.hemiLength!==m||S.numDirectionalShadows!==h||S.numPointShadows!==g||S.numSpotShadows!==_||S.numSpotMaps!==v||S.numLightProbes!==b)&&(i.directional.length=u,i.spot.length=f,i.rectArea.length=p,i.point.length=d,i.hemi.length=m,i.directionalShadow.length=h,i.directionalShadowMap.length=h,i.pointShadow.length=g,i.pointShadowMap.length=g,i.spotShadow.length=_,i.spotShadowMap.length=_,i.directionalShadowMatrix.length=h,i.pointShadowMatrix.length=g,i.spotLightMatrix.length=_+v-y,i.spotLightMap.length=v,i.numSpotLightShadowsWithMaps=y,i.numLightProbes=b,S.directionalLength=u,S.pointLength=d,S.spotLength=f,S.rectAreaLength=p,S.hemiLength=m,S.numDirectionalShadows=h,S.numPointShadows=g,S.numSpotShadows=_,S.numSpotMaps=v,S.numLightProbes=b,i.version=gl++)}function l(e,t){let n=0,r=0,c=0,l=0,u=0,d=t.matrixWorldInverse;for(let t=0,f=e.length;t<f;t++){let f=e[t];if(f.isDirectionalLight){let e=i.directional[n];e.direction.setFromMatrixPosition(f.matrixWorld),a.setFromMatrixPosition(f.target.matrixWorld),e.direction.sub(a),e.direction.transformDirection(d),n++}else if(f.isSpotLight){let e=i.spot[c];e.position.setFromMatrixPosition(f.matrixWorld),e.position.applyMatrix4(d),e.direction.setFromMatrixPosition(f.matrixWorld),a.setFromMatrixPosition(f.target.matrixWorld),e.direction.sub(a),e.direction.transformDirection(d),c++}else if(f.isRectAreaLight){let e=i.rectArea[l];e.position.setFromMatrixPosition(f.matrixWorld),e.position.applyMatrix4(d),s.identity(),o.copy(f.matrixWorld),o.premultiply(d),s.extractRotation(o),e.halfWidth.set(f.width*.5,0,0),e.halfHeight.set(0,f.height*.5,0),e.halfWidth.applyMatrix4(s),e.halfHeight.applyMatrix4(s),l++}else if(f.isPointLight){let e=i.point[r];e.position.setFromMatrixPosition(f.matrixWorld),e.position.applyMatrix4(d),r++}else if(f.isHemisphereLight){let e=i.hemi[u];e.direction.setFromMatrixPosition(f.matrixWorld),e.direction.transformDirection(d),u++}}}return{setup:c,setupView:l,state:i}}function yl(e,t){let n=new vl(e,t),r=[],i=[];function a(){r.length=0,i.length=0}function o(e){r.push(e)}function s(e){i.push(e)}function c(e){n.setup(r,e)}function l(e){n.setupView(r,e)}return{init:a,state:{lightsArray:r,shadowsArray:i,lights:n},setupLights:c,setupLightsView:l,pushLight:o,pushShadow:s}}function bl(e,t){let n=new WeakMap;function r(r,i=0){let a=n.get(r),o;return a===void 0?(o=new yl(e,t),n.set(r,[o])):i>=a.length?(o=new yl(e,t),a.push(o)):o=a[i],o}function i(){n=new WeakMap}return{get:r,dispose:i}}var xl=class extends Ta{constructor(e){super(),this.isMeshDepthMaterial=!0,this.type=`MeshDepthMaterial`,this.depthPacking=Gn,this.map=null,this.alphaMap=null,this.displacementMap=null,this.displacementScale=1,this.displacementBias=0,this.wireframe=!1,this.wireframeLinewidth=1,this.setValues(e)}copy(e){return super.copy(e),this.depthPacking=e.depthPacking,this.map=e.map,this.alphaMap=e.alphaMap,this.displacementMap=e.displacementMap,this.displacementScale=e.displacementScale,this.displacementBias=e.displacementBias,this.wireframe=e.wireframe,this.wireframeLinewidth=e.wireframeLinewidth,this}},Sl=class extends Ta{constructor(e){super(),this.isMeshDistanceMaterial=!0,this.type=`MeshDistanceMaterial`,this.map=null,this.alphaMap=null,this.displacementMap=null,this.displacementScale=1,this.displacementBias=0,this.setValues(e)}copy(e){return super.copy(e),this.map=e.map,this.alphaMap=e.alphaMap,this.displacementMap=e.displacementMap,this.displacementScale=e.displacementScale,this.displacementBias=e.displacementBias,this}},Cl=`void main() {
	gl_Position = vec4( position, 1.0 );
}`,wl=`uniform sampler2D shadow_pass;
uniform vec2 resolution;
uniform float radius;
#include <packing>
void main() {
	const float samples = float( VSM_SAMPLES );
	float mean = 0.0;
	float squared_mean = 0.0;
	float uvStride = samples <= 1.0 ? 0.0 : 2.0 / ( samples - 1.0 );
	float uvStart = samples <= 1.0 ? 0.0 : - 1.0;
	for ( float i = 0.0; i < samples; i ++ ) {
		float uvOffset = uvStart + i * uvStride;
		#ifdef HORIZONTAL_PASS
			vec2 distribution = unpackRGBATo2Half( texture2D( shadow_pass, ( gl_FragCoord.xy + vec2( uvOffset, 0.0 ) * radius ) / resolution ) );
			mean += distribution.x;
			squared_mean += distribution.y * distribution.y + distribution.x * distribution.x;
		#else
			float depth = unpackRGBAToDepth( texture2D( shadow_pass, ( gl_FragCoord.xy + vec2( 0.0, uvOffset ) * radius ) / resolution ) );
			mean += depth;
			squared_mean += depth * depth;
		#endif
	}
	mean = mean / samples;
	squared_mean = squared_mean / samples;
	float std_dev = sqrt( squared_mean - mean * mean );
	gl_FragColor = pack2HalfToRGBA( vec2( mean, std_dev ) );
}`;function Tl(e,t,n){let r=new No,i=new Y,a=new Y,o=new Qr,s=new xl({depthPacking:Kn}),c=new Sl,l={},u=n.maxTextureSize,d={0:1,1:0,2:2},f=new go({defines:{VSM_SAMPLES:8},uniforms:{shadow_pass:{value:null},resolution:{value:new Y},radius:{value:4}},vertexShader:Cl,fragmentShader:wl}),p=f.clone();p.defines.HORIZONTAL_PASS=1;let m=new Ba;m.setAttribute(`position`,new ka(new Float32Array([-1,-1,.5,3,-1,.5,-1,3,.5]),3));let h=new io(m,f),g=this;this.enabled=!1,this.autoUpdate=!0,this.needsUpdate=!1,this.type=1;let _=this.type;this.render=function(t,n,s){if(g.enabled===!1||g.autoUpdate===!1&&g.needsUpdate===!1||t.length===0)return;let c=e.getRenderTarget(),l=e.getActiveCubeFace(),d=e.getActiveMipmapLevel(),f=e.state;f.setBlending(0),f.buffers.color.setClear(1,1,1,1),f.buffers.depth.setTest(!0),f.setScissorTest(!1);let p=_!==3&&this.type===3,m=_===3&&this.type!==3;for(let c=0,l=t.length;c<l;c++){let l=t[c],d=l.shadow;if(d===void 0){console.warn(`THREE.WebGLShadowMap:`,l,`has no shadow.`);continue}if(d.autoUpdate===!1&&d.needsUpdate===!1)continue;i.copy(d.mapSize);let h=d.getFrameExtents();if(i.multiply(h),a.copy(d.mapSize),(i.x>u||i.y>u)&&(i.x>u&&(a.x=Math.floor(u/h.x),i.x=a.x*h.x,d.mapSize.x=a.x),i.y>u&&(a.y=Math.floor(u/h.y),i.y=a.y*h.y,d.mapSize.y=a.y)),d.map===null||p===!0||m===!0){let e=this.type===3?{}:{minFilter:Tn,magFilter:Tn};d.map!==null&&d.map.dispose(),d.map=new ei(i.x,i.y,e),d.map.texture.name=l.name+`.shadowMap`,d.camera.updateProjectionMatrix()}e.setRenderTarget(d.map),e.clear();let g=d.getViewportCount();for(let e=0;e<g;e++){let t=d.getViewport(e);o.set(a.x*t.x,a.y*t.y,a.x*t.z,a.y*t.w),f.viewport(o),d.updateMatrices(l,e),r=d.getFrustum(),b(n,s,d.camera,l,this.type)}d.isPointLightShadow!==!0&&this.type===3&&v(d,s),d.needsUpdate=!1}_=this.type,g.needsUpdate=!1,e.setRenderTarget(c,l,d)};function v(n,r){let a=t.update(h);f.defines.VSM_SAMPLES!==n.blurSamples&&(f.defines.VSM_SAMPLES=n.blurSamples,p.defines.VSM_SAMPLES=n.blurSamples,f.needsUpdate=!0,p.needsUpdate=!0),n.mapPass===null&&(n.mapPass=new ei(i.x,i.y)),f.uniforms.shadow_pass.value=n.map.texture,f.uniforms.resolution.value=n.mapSize,f.uniforms.radius.value=n.radius,e.setRenderTarget(n.mapPass),e.clear(),e.renderBufferDirect(r,null,a,f,h,null),p.uniforms.shadow_pass.value=n.mapPass.texture,p.uniforms.resolution.value=n.mapSize,p.uniforms.radius.value=n.radius,e.setRenderTarget(n.map),e.clear(),e.renderBufferDirect(r,null,a,p,h,null)}function y(t,n,r,i){let a=null,o=r.isPointLight===!0?t.customDistanceMaterial:t.customDepthMaterial;if(o!==void 0)a=o;else if(a=r.isPointLight===!0?c:s,e.localClippingEnabled&&n.clipShadows===!0&&Array.isArray(n.clippingPlanes)&&n.clippingPlanes.length!==0||n.displacementMap&&n.displacementScale!==0||n.alphaMap&&n.alphaTest>0||n.map&&n.alphaTest>0){let e=a.uuid,t=n.uuid,r=l[e];r===void 0&&(r={},l[e]=r);let i=r[t];i===void 0&&(i=a.clone(),r[t]=i,n.addEventListener(`dispose`,x)),a=i}if(a.visible=n.visible,a.wireframe=n.wireframe,i===3?a.side=n.shadowSide===null?n.side:n.shadowSide:a.side=n.shadowSide===null?d[n.side]:n.shadowSide,a.alphaMap=n.alphaMap,a.alphaTest=n.alphaTest,a.map=n.map,a.clipShadows=n.clipShadows,a.clippingPlanes=n.clippingPlanes,a.clipIntersection=n.clipIntersection,a.displacementMap=n.displacementMap,a.displacementScale=n.displacementScale,a.displacementBias=n.displacementBias,a.wireframeLinewidth=n.wireframeLinewidth,a.linewidth=n.linewidth,r.isPointLight===!0&&a.isMeshDistanceMaterial===!0){let t=e.properties.get(a);t.light=r}return a}function b(n,i,a,o,s){if(n.visible===!1)return;if(n.layers.test(i.layers)&&(n.isMesh||n.isLine||n.isPoints)&&(n.castShadow||n.receiveShadow&&s===3)&&(!n.frustumCulled||r.intersectsObject(n))){n.modelViewMatrix.multiplyMatrices(a.matrixWorldInverse,n.matrixWorld);let r=t.update(n),c=n.material;if(Array.isArray(c)){let t=r.groups;for(let l=0,u=t.length;l<u;l++){let u=t[l],d=c[u.materialIndex];if(d&&d.visible){let t=y(n,d,o,s);n.onBeforeShadow(e,n,i,a,r,t,u),e.renderBufferDirect(a,null,r,t,n,u),n.onAfterShadow(e,n,i,a,r,t,u)}}}else if(c.visible){let t=y(n,c,o,s);n.onBeforeShadow(e,n,i,a,r,t,null),e.renderBufferDirect(a,null,r,t,n,null),n.onAfterShadow(e,n,i,a,r,t,null)}}let c=n.children;for(let e=0,t=c.length;e<t;e++)b(c[e],i,a,o,s)}function x(e){e.target.removeEventListener(`dispose`,x);for(let t in l){let n=l[t],r=e.target.uuid;r in n&&(n[r].dispose(),delete n[r])}}}function El(e,t,n){let r=n.isWebGL2;function i(){let t=!1,n=new Qr,r=null,i=new Qr(0,0,0,0);return{setMask:function(n){r!==n&&!t&&(e.colorMask(n,n,n,n),r=n)},setLocked:function(e){t=e},setClear:function(t,r,a,o,s){s===!0&&(t*=o,r*=o,a*=o),n.set(t,r,a,o),i.equals(n)===!1&&(e.clearColor(t,r,a,o),i.copy(n))},reset:function(){t=!1,r=null,i.set(-1,0,0,0)}}}function a(){let t=!1,n=null,r=null,i=null;return{setTest:function(t){t?se(e.DEPTH_TEST):ce(e.DEPTH_TEST)},setMask:function(r){n!==r&&!t&&(e.depthMask(r),n=r)},setFunc:function(t){if(r!==t){switch(t){case 0:e.depthFunc(e.NEVER);break;case 1:e.depthFunc(e.ALWAYS);break;case 2:e.depthFunc(e.LESS);break;case 3:e.depthFunc(e.LEQUAL);break;case 4:e.depthFunc(e.EQUAL);break;case 5:e.depthFunc(e.GEQUAL);break;case 6:e.depthFunc(e.GREATER);break;case 7:e.depthFunc(e.NOTEQUAL);break;default:e.depthFunc(e.LEQUAL)}r=t}},setLocked:function(e){t=e},setClear:function(t){i!==t&&(e.clearDepth(t),i=t)},reset:function(){t=!1,n=null,r=null,i=null}}}function o(){let t=!1,n=null,r=null,i=null,a=null,o=null,s=null,c=null,l=null;return{setTest:function(n){t||(n?se(e.STENCIL_TEST):ce(e.STENCIL_TEST))},setMask:function(r){n!==r&&!t&&(e.stencilMask(r),n=r)},setFunc:function(t,n,o){(r!==t||i!==n||a!==o)&&(e.stencilFunc(t,n,o),r=t,i=n,a=o)},setOp:function(t,n,r){(o!==t||s!==n||c!==r)&&(e.stencilOp(t,n,r),o=t,s=n,c=r)},setLocked:function(e){t=e},setClear:function(t){l!==t&&(e.clearStencil(t),l=t)},reset:function(){t=!1,n=null,r=null,i=null,a=null,o=null,s=null,c=null,l=null}}}let s=new i,c=new a,l=new o,u=new WeakMap,d=new WeakMap,f={},p={},m=new WeakMap,h=[],g=null,_=!1,v=null,y=null,b=null,x=null,S=null,C=null,w=null,T=new Sa(0,0,0),E=0,D=!1,O=null,k=null,A=null,j=null,M=null,N=e.getParameter(e.MAX_COMBINED_TEXTURE_IMAGE_UNITS),P=!1,F=0,ee=e.getParameter(e.VERSION);ee.indexOf(`WebGL`)===-1?ee.indexOf(`OpenGL ES`)!==-1&&(F=parseFloat(/^OpenGL ES (\d)/.exec(ee)[1]),P=F>=2):(F=parseFloat(/^WebGL (\d)/.exec(ee)[1]),P=F>=1);let I=null,te={},ne=e.getParameter(e.SCISSOR_BOX),re=e.getParameter(e.VIEWPORT),ie=new Qr().fromArray(ne),ae=new Qr().fromArray(re);function oe(t,n,i,a){let o=new Uint8Array(4),s=e.createTexture();e.bindTexture(t,s),e.texParameteri(t,e.TEXTURE_MIN_FILTER,e.NEAREST),e.texParameteri(t,e.TEXTURE_MAG_FILTER,e.NEAREST);for(let s=0;s<i;s++)r&&(t===e.TEXTURE_3D||t===e.TEXTURE_2D_ARRAY)?e.texImage3D(n,0,e.RGBA,1,1,a,0,e.RGBA,e.UNSIGNED_BYTE,o):e.texImage2D(n+s,0,e.RGBA,1,1,0,e.RGBA,e.UNSIGNED_BYTE,o);return s}let L={};L[e.TEXTURE_2D]=oe(e.TEXTURE_2D,e.TEXTURE_2D,1),L[e.TEXTURE_CUBE_MAP]=oe(e.TEXTURE_CUBE_MAP,e.TEXTURE_CUBE_MAP_POSITIVE_X,6),r&&(L[e.TEXTURE_2D_ARRAY]=oe(e.TEXTURE_2D_ARRAY,e.TEXTURE_2D_ARRAY,1,1),L[e.TEXTURE_3D]=oe(e.TEXTURE_3D,e.TEXTURE_3D,1,1)),s.setClear(0,0,0,1),c.setClear(1),l.setClear(0),se(e.DEPTH_TEST),c.setFunc(3),W(!1),ue(1),se(e.CULL_FACE),H(0);function se(t){f[t]!==!0&&(e.enable(t),f[t]=!0)}function ce(t){f[t]!==!1&&(e.disable(t),f[t]=!1)}function R(t,n){return p[t]===n?!1:(e.bindFramebuffer(t,n),p[t]=n,r&&(t===e.DRAW_FRAMEBUFFER&&(p[e.FRAMEBUFFER]=n),t===e.FRAMEBUFFER&&(p[e.DRAW_FRAMEBUFFER]=n)),!0)}function le(r,i){let a=h,o=!1;if(r){a=m.get(i),a===void 0&&(a=[],m.set(i,a));let t=r.textures;if(a.length!==t.length||a[0]!==e.COLOR_ATTACHMENT0){for(let n=0,r=t.length;n<r;n++)a[n]=e.COLOR_ATTACHMENT0+n;a.length=t.length,o=!0}}else a[0]!==e.BACK&&(a[0]=e.BACK,o=!0);if(o)if(n.isWebGL2)e.drawBuffers(a);else if(t.has(`WEBGL_draw_buffers`)===!0)t.get(`WEBGL_draw_buffers`).drawBuffersWEBGL(a);else throw Error(`THREE.WebGLState: Usage of gl.drawBuffers() require WebGL2 or WEBGL_draw_buffers extension`)}function z(t){return g===t?!1:(e.useProgram(t),g=t,!0)}let B={100:e.FUNC_ADD,101:e.FUNC_SUBTRACT,102:e.FUNC_REVERSE_SUBTRACT};if(r)B[103]=e.MIN,B[104]=e.MAX;else{let e=t.get(`EXT_blend_minmax`);e!==null&&(B[103]=e.MIN_EXT,B[104]=e.MAX_EXT)}let V={200:e.ZERO,201:e.ONE,202:e.SRC_COLOR,204:e.SRC_ALPHA,210:e.SRC_ALPHA_SATURATE,208:e.DST_COLOR,206:e.DST_ALPHA,203:e.ONE_MINUS_SRC_COLOR,205:e.ONE_MINUS_SRC_ALPHA,209:e.ONE_MINUS_DST_COLOR,207:e.ONE_MINUS_DST_ALPHA,211:e.CONSTANT_COLOR,212:e.ONE_MINUS_CONSTANT_COLOR,213:e.CONSTANT_ALPHA,214:e.ONE_MINUS_CONSTANT_ALPHA};function H(t,n,r,i,a,o,s,c,l,u){if(t===0){_===!0&&(ce(e.BLEND),_=!1);return}if(_===!1&&(se(e.BLEND),_=!0),t!==5){if(t!==v||u!==D){if((y!==100||S!==100)&&(e.blendEquation(e.FUNC_ADD),y=100,S=100),u)switch(t){case 1:e.blendFuncSeparate(e.ONE,e.ONE_MINUS_SRC_ALPHA,e.ONE,e.ONE_MINUS_SRC_ALPHA);break;case 2:e.blendFunc(e.ONE,e.ONE);break;case 3:e.blendFuncSeparate(e.ZERO,e.ONE_MINUS_SRC_COLOR,e.ZERO,e.ONE);break;case 4:e.blendFuncSeparate(e.ZERO,e.SRC_COLOR,e.ZERO,e.SRC_ALPHA);break;default:console.error(`THREE.WebGLState: Invalid blending: `,t);break}else switch(t){case 1:e.blendFuncSeparate(e.SRC_ALPHA,e.ONE_MINUS_SRC_ALPHA,e.ONE,e.ONE_MINUS_SRC_ALPHA);break;case 2:e.blendFunc(e.SRC_ALPHA,e.ONE);break;case 3:e.blendFuncSeparate(e.ZERO,e.ONE_MINUS_SRC_COLOR,e.ZERO,e.ONE);break;case 4:e.blendFunc(e.ZERO,e.SRC_COLOR);break;default:console.error(`THREE.WebGLState: Invalid blending: `,t);break}b=null,x=null,C=null,w=null,T.set(0,0,0),E=0,v=t,D=u}return}a||=n,o||=r,s||=i,(n!==y||a!==S)&&(e.blendEquationSeparate(B[n],B[a]),y=n,S=a),(r!==b||i!==x||o!==C||s!==w)&&(e.blendFuncSeparate(V[r],V[i],V[o],V[s]),b=r,x=i,C=o,w=s),(c.equals(T)===!1||l!==E)&&(e.blendColor(c.r,c.g,c.b,l),T.copy(c),E=l),v=t,D=!1}function U(t,n){t.side===2?ce(e.CULL_FACE):se(e.CULL_FACE);let r=t.side===1;n&&(r=!r),W(r),t.blending===1&&t.transparent===!1?H(0):H(t.blending,t.blendEquation,t.blendSrc,t.blendDst,t.blendEquationAlpha,t.blendSrcAlpha,t.blendDstAlpha,t.blendColor,t.blendAlpha,t.premultipliedAlpha),c.setFunc(t.depthFunc),c.setTest(t.depthTest),c.setMask(t.depthWrite),s.setMask(t.colorWrite);let i=t.stencilWrite;l.setTest(i),i&&(l.setMask(t.stencilWriteMask),l.setFunc(t.stencilFunc,t.stencilRef,t.stencilFuncMask),l.setOp(t.stencilFail,t.stencilZFail,t.stencilZPass)),fe(t.polygonOffset,t.polygonOffsetFactor,t.polygonOffsetUnits),t.alphaToCoverage===!0?se(e.SAMPLE_ALPHA_TO_COVERAGE):ce(e.SAMPLE_ALPHA_TO_COVERAGE)}function W(t){O!==t&&(t?e.frontFace(e.CW):e.frontFace(e.CCW),O=t)}function ue(t){t===0?ce(e.CULL_FACE):(se(e.CULL_FACE),t!==k&&(t===1?e.cullFace(e.BACK):t===2?e.cullFace(e.FRONT):e.cullFace(e.FRONT_AND_BACK))),k=t}function de(t){t!==A&&(P&&e.lineWidth(t),A=t)}function fe(t,n,r){t?(se(e.POLYGON_OFFSET_FILL),(j!==n||M!==r)&&(e.polygonOffset(n,r),j=n,M=r)):ce(e.POLYGON_OFFSET_FILL)}function pe(t){t?se(e.SCISSOR_TEST):ce(e.SCISSOR_TEST)}function me(t){t===void 0&&(t=e.TEXTURE0+N-1),I!==t&&(e.activeTexture(t),I=t)}function he(t,n,r){r===void 0&&(r=I===null?e.TEXTURE0+N-1:I);let i=te[r];i===void 0&&(i={type:void 0,texture:void 0},te[r]=i),(i.type!==t||i.texture!==n)&&(I!==r&&(e.activeTexture(r),I=r),e.bindTexture(t,n||L[t]),i.type=t,i.texture=n)}function ge(){let t=te[I];t!==void 0&&t.type!==void 0&&(e.bindTexture(t.type,null),t.type=void 0,t.texture=void 0)}function _e(){try{e.compressedTexImage2D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function ve(){try{e.compressedTexImage3D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function ye(){try{e.texSubImage2D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function be(){try{e.texSubImage3D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function xe(){try{e.compressedTexSubImage2D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function Se(){try{e.compressedTexSubImage3D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function G(){try{e.texStorage2D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function Ce(){try{e.texStorage3D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function we(){try{e.texImage2D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function Te(){try{e.texImage3D.apply(e,arguments)}catch(e){console.error(`THREE.WebGLState:`,e)}}function Ee(t){ie.equals(t)===!1&&(e.scissor(t.x,t.y,t.z,t.w),ie.copy(t))}function De(t){ae.equals(t)===!1&&(e.viewport(t.x,t.y,t.z,t.w),ae.copy(t))}function Oe(t,n){let r=d.get(n);r===void 0&&(r=new WeakMap,d.set(n,r));let i=r.get(t);i===void 0&&(i=e.getUniformBlockIndex(n,t.name),r.set(t,i))}function ke(t,n){let r=d.get(n).get(t);u.get(n)!==r&&(e.uniformBlockBinding(n,r,t.__bindingPointIndex),u.set(n,r))}function Ae(){e.disable(e.BLEND),e.disable(e.CULL_FACE),e.disable(e.DEPTH_TEST),e.disable(e.POLYGON_OFFSET_FILL),e.disable(e.SCISSOR_TEST),e.disable(e.STENCIL_TEST),e.disable(e.SAMPLE_ALPHA_TO_COVERAGE),e.blendEquation(e.FUNC_ADD),e.blendFunc(e.ONE,e.ZERO),e.blendFuncSeparate(e.ONE,e.ZERO,e.ONE,e.ZERO),e.blendColor(0,0,0,0),e.colorMask(!0,!0,!0,!0),e.clearColor(0,0,0,0),e.depthMask(!0),e.depthFunc(e.LESS),e.clearDepth(1),e.stencilMask(4294967295),e.stencilFunc(e.ALWAYS,0,4294967295),e.stencilOp(e.KEEP,e.KEEP,e.KEEP),e.clearStencil(0),e.cullFace(e.BACK),e.frontFace(e.CCW),e.polygonOffset(0,0),e.activeTexture(e.TEXTURE0),e.bindFramebuffer(e.FRAMEBUFFER,null),r===!0&&(e.bindFramebuffer(e.DRAW_FRAMEBUFFER,null),e.bindFramebuffer(e.READ_FRAMEBUFFER,null)),e.useProgram(null),e.lineWidth(1),e.scissor(0,0,e.canvas.width,e.canvas.height),e.viewport(0,0,e.canvas.width,e.canvas.height),f={},I=null,te={},p={},m=new WeakMap,h=[],g=null,_=!1,v=null,y=null,b=null,x=null,S=null,C=null,w=null,T=new Sa(0,0,0),E=0,D=!1,O=null,k=null,A=null,j=null,M=null,ie.set(0,0,e.canvas.width,e.canvas.height),ae.set(0,0,e.canvas.width,e.canvas.height),s.reset(),c.reset(),l.reset()}return{buffers:{color:s,depth:c,stencil:l},enable:se,disable:ce,bindFramebuffer:R,drawBuffers:le,useProgram:z,setBlending:H,setMaterial:U,setFlipSided:W,setCullFace:ue,setLineWidth:de,setPolygonOffset:fe,setScissorTest:pe,activeTexture:me,bindTexture:he,unbindTexture:ge,compressedTexImage2D:_e,compressedTexImage3D:ve,texImage2D:we,texImage3D:Te,updateUBOMapping:Oe,uniformBlockBinding:ke,texStorage2D:G,texStorage3D:Ce,texSubImage2D:ye,texSubImage3D:be,compressedTexSubImage2D:xe,compressedTexSubImage3D:Se,scissor:Ee,viewport:De,reset:Ae}}function Dl(e,t,n,r,i,a,o){let s=i.isWebGL2,c=t.has(`WEBGL_multisampled_render_to_texture`)?t.get(`WEBGL_multisampled_render_to_texture`):null,l=typeof navigator>`u`?!1:/OculusBrowser/g.test(navigator.userAgent),u=new Y,d=new WeakMap,f,p=new WeakMap,m=!1;try{m=typeof OffscreenCanvas<`u`&&new OffscreenCanvas(1,1).getContext(`2d`)!==null}catch{}function h(e,t){return m?new OffscreenCanvas(e,t):Pr(`canvas`)}function g(e,t,n,r){let i=1,a=de(e);if((a.width>r||a.height>r)&&(i=r/Math.max(a.width,a.height)),i<1||t===!0)if(typeof HTMLImageElement<`u`&&e instanceof HTMLImageElement||typeof HTMLCanvasElement<`u`&&e instanceof HTMLCanvasElement||typeof ImageBitmap<`u`&&e instanceof ImageBitmap||typeof VideoFrame<`u`&&e instanceof VideoFrame){let r=t?Dr:Math.floor,o=r(i*a.width),s=r(i*a.height);f===void 0&&(f=h(o,s));let c=n?h(o,s):f;return c.width=o,c.height=s,c.getContext(`2d`).drawImage(e,0,0,o,s),console.warn(`THREE.WebGLRenderer: Texture has been resized from (`+a.width+`x`+a.height+`) to (`+o+`x`+s+`).`),c}else return`data`in e&&console.warn(`THREE.WebGLRenderer: Image in DataTexture is too big (`+a.width+`x`+a.height+`).`),e;return e}function _(e){let t=de(e);return Tr(t.width)&&Tr(t.height)}function v(e){return s?!1:e.wrapS!==1001||e.wrapT!==1001||e.minFilter!==1003&&e.minFilter!==1006}function y(e,t){return e.generateMipmaps&&t&&e.minFilter!==1003&&e.minFilter!==1006}function b(t){e.generateMipmap(t)}function x(n,r,i,a,o=!1){if(s===!1)return r;if(n!==null){if(e[n]!==void 0)return e[n];console.warn(`THREE.WebGLRenderer: Attempt to use non-existing WebGL internal format '`+n+`'`)}let c=r;if(r===e.RED&&(i===e.FLOAT&&(c=e.R32F),i===e.HALF_FLOAT&&(c=e.R16F),i===e.UNSIGNED_BYTE&&(c=e.R8)),r===e.RED_INTEGER&&(i===e.UNSIGNED_BYTE&&(c=e.R8UI),i===e.UNSIGNED_SHORT&&(c=e.R16UI),i===e.UNSIGNED_INT&&(c=e.R32UI),i===e.BYTE&&(c=e.R8I),i===e.SHORT&&(c=e.R16I),i===e.INT&&(c=e.R32I)),r===e.RG&&(i===e.FLOAT&&(c=e.RG32F),i===e.HALF_FLOAT&&(c=e.RG16F),i===e.UNSIGNED_BYTE&&(c=e.RG8)),r===e.RG_INTEGER&&(i===e.UNSIGNED_BYTE&&(c=e.RG8UI),i===e.UNSIGNED_SHORT&&(c=e.RG16UI),i===e.UNSIGNED_INT&&(c=e.RG32UI),i===e.BYTE&&(c=e.RG8I),i===e.SHORT&&(c=e.RG16I),i===e.INT&&(c=e.RG32I)),r===e.RGBA){let t=o?Zn:Hr.getTransfer(a);i===e.FLOAT&&(c=e.RGBA32F),i===e.HALF_FLOAT&&(c=e.RGBA16F),i===e.UNSIGNED_BYTE&&(c=t===`srgb`?e.SRGB8_ALPHA8:e.RGBA8),i===e.UNSIGNED_SHORT_4_4_4_4&&(c=e.RGBA4),i===e.UNSIGNED_SHORT_5_5_5_1&&(c=e.RGB5_A1)}return(c===e.R16F||c===e.R32F||c===e.RG16F||c===e.RG32F||c===e.RGBA16F||c===e.RGBA32F)&&t.get(`EXT_color_buffer_float`),c}function S(e,t,n){return y(e,n)===!0||e.isFramebufferTexture&&e.minFilter!==1003&&e.minFilter!==1006?Math.log2(Math.max(t.width,t.height))+1:e.mipmaps!==void 0&&e.mipmaps.length>0?e.mipmaps.length:e.isCompressedTexture&&Array.isArray(e.image)?t.mipmaps.length:1}function C(t){return t===1003||t===1004||t===1005?e.NEAREST:e.LINEAR}function w(e){let t=e.target;t.removeEventListener(`dispose`,w),E(t),t.isVideoTexture&&d.delete(t)}function T(e){let t=e.target;t.removeEventListener(`dispose`,T),O(t)}function E(e){let t=r.get(e);if(t.__webglInit===void 0)return;let n=e.source,i=p.get(n);if(i){let r=i[t.__cacheKey];r.usedTimes--,r.usedTimes===0&&D(e),Object.keys(i).length===0&&p.delete(n)}r.remove(e)}function D(t){let n=r.get(t);e.deleteTexture(n.__webglTexture);let i=t.source,a=p.get(i);delete a[n.__cacheKey],o.memory.textures--}function O(t){let n=r.get(t);if(t.depthTexture&&t.depthTexture.dispose(),t.isWebGLCubeRenderTarget)for(let t=0;t<6;t++){if(Array.isArray(n.__webglFramebuffer[t]))for(let r=0;r<n.__webglFramebuffer[t].length;r++)e.deleteFramebuffer(n.__webglFramebuffer[t][r]);else e.deleteFramebuffer(n.__webglFramebuffer[t]);n.__webglDepthbuffer&&e.deleteRenderbuffer(n.__webglDepthbuffer[t])}else{if(Array.isArray(n.__webglFramebuffer))for(let t=0;t<n.__webglFramebuffer.length;t++)e.deleteFramebuffer(n.__webglFramebuffer[t]);else e.deleteFramebuffer(n.__webglFramebuffer);if(n.__webglDepthbuffer&&e.deleteRenderbuffer(n.__webglDepthbuffer),n.__webglMultisampledFramebuffer&&e.deleteFramebuffer(n.__webglMultisampledFramebuffer),n.__webglColorRenderbuffer)for(let t=0;t<n.__webglColorRenderbuffer.length;t++)n.__webglColorRenderbuffer[t]&&e.deleteRenderbuffer(n.__webglColorRenderbuffer[t]);n.__webglDepthRenderbuffer&&e.deleteRenderbuffer(n.__webglDepthRenderbuffer)}let i=t.textures;for(let t=0,n=i.length;t<n;t++){let n=r.get(i[t]);n.__webglTexture&&(e.deleteTexture(n.__webglTexture),o.memory.textures--),r.remove(i[t])}r.remove(t)}let k=0;function A(){k=0}function j(){let e=k;return e>=i.maxTextures&&console.warn(`THREE.WebGLTextures: Trying to use `+e+` texture units while this GPU supports only `+i.maxTextures),k+=1,e}function M(e){let t=[];return t.push(e.wrapS),t.push(e.wrapT),t.push(e.wrapR||0),t.push(e.magFilter),t.push(e.minFilter),t.push(e.anisotropy),t.push(e.internalFormat),t.push(e.format),t.push(e.type),t.push(e.generateMipmaps),t.push(e.premultiplyAlpha),t.push(e.flipY),t.push(e.unpackAlignment),t.push(e.colorSpace),t.join()}function N(t,i){let a=r.get(t);if(t.isVideoTexture&&W(t),t.isRenderTargetTexture===!1&&t.version>0&&a.__version!==t.version){let e=t.image;if(e===null)console.warn(`THREE.WebGLRenderer: Texture marked for update but no image data found.`);else if(e.complete===!1)console.warn(`THREE.WebGLRenderer: Texture marked for update but image is incomplete`);else{ae(a,t,i);return}}n.bindTexture(e.TEXTURE_2D,a.__webglTexture,e.TEXTURE0+i)}function P(t,i){let a=r.get(t);if(t.version>0&&a.__version!==t.version){ae(a,t,i);return}n.bindTexture(e.TEXTURE_2D_ARRAY,a.__webglTexture,e.TEXTURE0+i)}function F(t,i){let a=r.get(t);if(t.version>0&&a.__version!==t.version){ae(a,t,i);return}n.bindTexture(e.TEXTURE_3D,a.__webglTexture,e.TEXTURE0+i)}function ee(t,i){let a=r.get(t);if(t.version>0&&a.__version!==t.version){oe(a,t,i);return}n.bindTexture(e.TEXTURE_CUBE_MAP,a.__webglTexture,e.TEXTURE0+i)}let I={[Sn]:e.REPEAT,[Cn]:e.CLAMP_TO_EDGE,[wn]:e.MIRRORED_REPEAT},te={[Tn]:e.NEAREST,[En]:e.NEAREST_MIPMAP_NEAREST,[Dn]:e.NEAREST_MIPMAP_LINEAR,[On]:e.LINEAR,[kn]:e.LINEAR_MIPMAP_NEAREST,[An]:e.LINEAR_MIPMAP_LINEAR},ne={512:e.NEVER,519:e.ALWAYS,513:e.LESS,515:e.LEQUAL,514:e.EQUAL,518:e.GEQUAL,516:e.GREATER,517:e.NOTEQUAL};function re(n,a,o){if(a.type===1015&&t.has(`OES_texture_float_linear`)===!1&&(a.magFilter===1006||a.magFilter===1007||a.magFilter===1005||a.magFilter===1008||a.minFilter===1006||a.minFilter===1007||a.minFilter===1005||a.minFilter===1008)&&console.warn(`THREE.WebGLRenderer: Unable to use linear filtering with floating point textures. OES_texture_float_linear not supported on this device.`),o?(e.texParameteri(n,e.TEXTURE_WRAP_S,I[a.wrapS]),e.texParameteri(n,e.TEXTURE_WRAP_T,I[a.wrapT]),(n===e.TEXTURE_3D||n===e.TEXTURE_2D_ARRAY)&&e.texParameteri(n,e.TEXTURE_WRAP_R,I[a.wrapR]),e.texParameteri(n,e.TEXTURE_MAG_FILTER,te[a.magFilter]),e.texParameteri(n,e.TEXTURE_MIN_FILTER,te[a.minFilter])):(e.texParameteri(n,e.TEXTURE_WRAP_S,e.CLAMP_TO_EDGE),e.texParameteri(n,e.TEXTURE_WRAP_T,e.CLAMP_TO_EDGE),(n===e.TEXTURE_3D||n===e.TEXTURE_2D_ARRAY)&&e.texParameteri(n,e.TEXTURE_WRAP_R,e.CLAMP_TO_EDGE),(a.wrapS!==1001||a.wrapT!==1001)&&console.warn(`THREE.WebGLRenderer: Texture is not power of two. Texture.wrapS and Texture.wrapT should be set to THREE.ClampToEdgeWrapping.`),e.texParameteri(n,e.TEXTURE_MAG_FILTER,C(a.magFilter)),e.texParameteri(n,e.TEXTURE_MIN_FILTER,C(a.minFilter)),a.minFilter!==1003&&a.minFilter!==1006&&console.warn(`THREE.WebGLRenderer: Texture is not power of two. Texture.minFilter should be set to THREE.NearestFilter or THREE.LinearFilter.`)),a.compareFunction&&(e.texParameteri(n,e.TEXTURE_COMPARE_MODE,e.COMPARE_REF_TO_TEXTURE),e.texParameteri(n,e.TEXTURE_COMPARE_FUNC,ne[a.compareFunction])),t.has(`EXT_texture_filter_anisotropic`)===!0){if(a.magFilter===1003||a.minFilter!==1005&&a.minFilter!==1008||a.type===1015&&t.has(`OES_texture_float_linear`)===!1||s===!1&&a.type===1016&&t.has(`OES_texture_half_float_linear`)===!1)return;if(a.anisotropy>1||r.get(a).__currentAnisotropy){let o=t.get(`EXT_texture_filter_anisotropic`);e.texParameterf(n,o.TEXTURE_MAX_ANISOTROPY_EXT,Math.min(a.anisotropy,i.getMaxAnisotropy())),r.get(a).__currentAnisotropy=a.anisotropy}}}function ie(t,n){let r=!1;t.__webglInit===void 0&&(t.__webglInit=!0,n.addEventListener(`dispose`,w));let i=n.source,a=p.get(i);a===void 0&&(a={},p.set(i,a));let s=M(n);if(s!==t.__cacheKey){a[s]===void 0&&(a[s]={texture:e.createTexture(),usedTimes:0},o.memory.textures++,r=!0),a[s].usedTimes++;let i=a[t.__cacheKey];i!==void 0&&(a[t.__cacheKey].usedTimes--,i.usedTimes===0&&D(n)),t.__cacheKey=s,t.__webglTexture=a[s].texture}return r}function ae(t,o,c){let l=e.TEXTURE_2D;(o.isDataArrayTexture||o.isCompressedArrayTexture)&&(l=e.TEXTURE_2D_ARRAY),o.isData3DTexture&&(l=e.TEXTURE_3D);let u=ie(t,o),d=o.source;n.bindTexture(l,t.__webglTexture,e.TEXTURE0+c);let f=r.get(d);if(d.version!==f.__version||u===!0){n.activeTexture(e.TEXTURE0+c);let t=Hr.getPrimaries(Hr.workingColorSpace),r=o.colorSpace===``?null:Hr.getPrimaries(o.colorSpace),p=o.colorSpace===``||t===r?e.NONE:e.BROWSER_DEFAULT_WEBGL;e.pixelStorei(e.UNPACK_FLIP_Y_WEBGL,o.flipY),e.pixelStorei(e.UNPACK_PREMULTIPLY_ALPHA_WEBGL,o.premultiplyAlpha),e.pixelStorei(e.UNPACK_ALIGNMENT,o.unpackAlignment),e.pixelStorei(e.UNPACK_COLORSPACE_CONVERSION_WEBGL,p);let m=v(o)&&_(o.image)===!1,h=g(o.image,m,!1,i.maxTextureSize);h=ue(o,h);let C=_(h)||s,w=a.convert(o.format,o.colorSpace),T=a.convert(o.type),E=x(o.internalFormat,w,T,o.colorSpace,o.isVideoTexture);re(l,o,C);let D,O=o.mipmaps,k=s&&o.isVideoTexture!==!0&&E!==36196,A=f.__version===void 0||u===!0,j=d.dataReady,M=S(o,h,C);if(o.isDepthTexture)E=e.DEPTH_COMPONENT,s?E=o.type===1015?e.DEPTH_COMPONENT32F:o.type===1014?e.DEPTH_COMPONENT24:o.type===1020?e.DEPTH24_STENCIL8:e.DEPTH_COMPONENT16:o.type===1015&&console.error(`WebGLRenderer: Floating point depth texture requires WebGL2.`),o.format===1026&&E===e.DEPTH_COMPONENT&&o.type!==1012&&o.type!==1014&&(console.warn(`THREE.WebGLRenderer: Use UnsignedShortType or UnsignedIntType for DepthFormat DepthTexture.`),o.type=Mn,T=a.convert(o.type)),o.format===1027&&E===e.DEPTH_COMPONENT&&(E=e.DEPTH_STENCIL,o.type!==1020&&(console.warn(`THREE.WebGLRenderer: Use UnsignedInt248Type for DepthStencilFormat DepthTexture.`),o.type=Fn,T=a.convert(o.type))),A&&(k?n.texStorage2D(e.TEXTURE_2D,1,E,h.width,h.height):n.texImage2D(e.TEXTURE_2D,0,E,h.width,h.height,0,w,T,null));else if(o.isDataTexture)if(O.length>0&&C){k&&A&&n.texStorage2D(e.TEXTURE_2D,M,E,O[0].width,O[0].height);for(let t=0,r=O.length;t<r;t++)D=O[t],k?j&&n.texSubImage2D(e.TEXTURE_2D,t,0,0,D.width,D.height,w,T,D.data):n.texImage2D(e.TEXTURE_2D,t,E,D.width,D.height,0,w,T,D.data);o.generateMipmaps=!1}else k?(A&&n.texStorage2D(e.TEXTURE_2D,M,E,h.width,h.height),j&&n.texSubImage2D(e.TEXTURE_2D,0,0,0,h.width,h.height,w,T,h.data)):n.texImage2D(e.TEXTURE_2D,0,E,h.width,h.height,0,w,T,h.data);else if(o.isCompressedTexture)if(o.isCompressedArrayTexture){k&&A&&n.texStorage3D(e.TEXTURE_2D_ARRAY,M,E,O[0].width,O[0].height,h.depth);for(let t=0,r=O.length;t<r;t++)D=O[t],o.format===1023?k?j&&n.texSubImage3D(e.TEXTURE_2D_ARRAY,t,0,0,0,D.width,D.height,h.depth,w,T,D.data):n.texImage3D(e.TEXTURE_2D_ARRAY,t,E,D.width,D.height,h.depth,0,w,T,D.data):w===null?console.warn(`THREE.WebGLRenderer: Attempt to load unsupported compressed texture format in .uploadTexture()`):k?j&&n.compressedTexSubImage3D(e.TEXTURE_2D_ARRAY,t,0,0,0,D.width,D.height,h.depth,w,D.data,0,0):n.compressedTexImage3D(e.TEXTURE_2D_ARRAY,t,E,D.width,D.height,h.depth,0,D.data,0,0)}else{k&&A&&n.texStorage2D(e.TEXTURE_2D,M,E,O[0].width,O[0].height);for(let t=0,r=O.length;t<r;t++)D=O[t],o.format===1023?k?j&&n.texSubImage2D(e.TEXTURE_2D,t,0,0,D.width,D.height,w,T,D.data):n.texImage2D(e.TEXTURE_2D,t,E,D.width,D.height,0,w,T,D.data):w===null?console.warn(`THREE.WebGLRenderer: Attempt to load unsupported compressed texture format in .uploadTexture()`):k?j&&n.compressedTexSubImage2D(e.TEXTURE_2D,t,0,0,D.width,D.height,w,D.data):n.compressedTexImage2D(e.TEXTURE_2D,t,E,D.width,D.height,0,D.data)}else if(o.isDataArrayTexture)k?(A&&n.texStorage3D(e.TEXTURE_2D_ARRAY,M,E,h.width,h.height,h.depth),j&&n.texSubImage3D(e.TEXTURE_2D_ARRAY,0,0,0,0,h.width,h.height,h.depth,w,T,h.data)):n.texImage3D(e.TEXTURE_2D_ARRAY,0,E,h.width,h.height,h.depth,0,w,T,h.data);else if(o.isData3DTexture)k?(A&&n.texStorage3D(e.TEXTURE_3D,M,E,h.width,h.height,h.depth),j&&n.texSubImage3D(e.TEXTURE_3D,0,0,0,0,h.width,h.height,h.depth,w,T,h.data)):n.texImage3D(e.TEXTURE_3D,0,E,h.width,h.height,h.depth,0,w,T,h.data);else if(o.isFramebufferTexture){if(A)if(k)n.texStorage2D(e.TEXTURE_2D,M,E,h.width,h.height);else{let t=h.width,r=h.height;for(let i=0;i<M;i++)n.texImage2D(e.TEXTURE_2D,i,E,t,r,0,w,T,null),t>>=1,r>>=1}}else if(O.length>0&&C){if(k&&A){let t=de(O[0]);n.texStorage2D(e.TEXTURE_2D,M,E,t.width,t.height)}for(let t=0,r=O.length;t<r;t++)D=O[t],k?j&&n.texSubImage2D(e.TEXTURE_2D,t,0,0,w,T,D):n.texImage2D(e.TEXTURE_2D,t,E,w,T,D);o.generateMipmaps=!1}else if(k){if(A){let t=de(h);n.texStorage2D(e.TEXTURE_2D,M,E,t.width,t.height)}j&&n.texSubImage2D(e.TEXTURE_2D,0,0,0,w,T,h)}else n.texImage2D(e.TEXTURE_2D,0,E,w,T,h);y(o,C)&&b(l),f.__version=d.version,o.onUpdate&&o.onUpdate(o)}t.__version=o.version}function oe(t,o,c){if(o.image.length!==6)return;let l=ie(t,o),u=o.source;n.bindTexture(e.TEXTURE_CUBE_MAP,t.__webglTexture,e.TEXTURE0+c);let d=r.get(u);if(u.version!==d.__version||l===!0){n.activeTexture(e.TEXTURE0+c);let t=Hr.getPrimaries(Hr.workingColorSpace),r=o.colorSpace===``?null:Hr.getPrimaries(o.colorSpace),f=o.colorSpace===``||t===r?e.NONE:e.BROWSER_DEFAULT_WEBGL;e.pixelStorei(e.UNPACK_FLIP_Y_WEBGL,o.flipY),e.pixelStorei(e.UNPACK_PREMULTIPLY_ALPHA_WEBGL,o.premultiplyAlpha),e.pixelStorei(e.UNPACK_ALIGNMENT,o.unpackAlignment),e.pixelStorei(e.UNPACK_COLORSPACE_CONVERSION_WEBGL,f);let p=o.isCompressedTexture||o.image[0].isCompressedTexture,m=o.image[0]&&o.image[0].isDataTexture,h=[];for(let e=0;e<6;e++)!p&&!m?h[e]=g(o.image[e],!1,!0,i.maxCubemapSize):h[e]=m?o.image[e].image:o.image[e],h[e]=ue(o,h[e]);let v=h[0],C=_(v)||s,w=a.convert(o.format,o.colorSpace),T=a.convert(o.type),E=x(o.internalFormat,w,T,o.colorSpace),D=s&&o.isVideoTexture!==!0,O=d.__version===void 0||l===!0,k=u.dataReady,A=S(o,v,C);re(e.TEXTURE_CUBE_MAP,o,C);let j;if(p){D&&O&&n.texStorage2D(e.TEXTURE_CUBE_MAP,A,E,v.width,v.height);for(let t=0;t<6;t++){j=h[t].mipmaps;for(let r=0;r<j.length;r++){let i=j[r];o.format===1023?D?k&&n.texSubImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r,0,0,i.width,i.height,w,T,i.data):n.texImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r,E,i.width,i.height,0,w,T,i.data):w===null?console.warn(`THREE.WebGLRenderer: Attempt to load unsupported compressed texture format in .setTextureCube()`):D?k&&n.compressedTexSubImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r,0,0,i.width,i.height,w,i.data):n.compressedTexImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r,E,i.width,i.height,0,i.data)}}}else{if(j=o.mipmaps,D&&O){j.length>0&&A++;let t=de(h[0]);n.texStorage2D(e.TEXTURE_CUBE_MAP,A,E,t.width,t.height)}for(let t=0;t<6;t++)if(m){D?k&&n.texSubImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,0,0,0,h[t].width,h[t].height,w,T,h[t].data):n.texImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,0,E,h[t].width,h[t].height,0,w,T,h[t].data);for(let r=0;r<j.length;r++){let i=j[r].image[t].image;D?k&&n.texSubImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r+1,0,0,i.width,i.height,w,T,i.data):n.texImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r+1,E,i.width,i.height,0,w,T,i.data)}}else{D?k&&n.texSubImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,0,0,0,w,T,h[t]):n.texImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,0,E,w,T,h[t]);for(let r=0;r<j.length;r++){let i=j[r];D?k&&n.texSubImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r+1,0,0,w,T,i.image[t]):n.texImage2D(e.TEXTURE_CUBE_MAP_POSITIVE_X+t,r+1,E,w,T,i.image[t])}}}y(o,C)&&b(e.TEXTURE_CUBE_MAP),d.__version=u.version,o.onUpdate&&o.onUpdate(o)}t.__version=o.version}function L(t,i,o,s,l,u){let d=a.convert(o.format,o.colorSpace),f=a.convert(o.type),p=x(o.internalFormat,d,f,o.colorSpace);if(!r.get(i).__hasExternalTextures){let t=Math.max(1,i.width>>u),r=Math.max(1,i.height>>u);l===e.TEXTURE_3D||l===e.TEXTURE_2D_ARRAY?n.texImage3D(l,u,p,t,r,i.depth,0,d,f,null):n.texImage2D(l,u,p,t,r,0,d,f,null)}n.bindFramebuffer(e.FRAMEBUFFER,t),U(i)?c.framebufferTexture2DMultisampleEXT(e.FRAMEBUFFER,s,l,r.get(o).__webglTexture,0,H(i)):(l===e.TEXTURE_2D||l>=e.TEXTURE_CUBE_MAP_POSITIVE_X&&l<=e.TEXTURE_CUBE_MAP_NEGATIVE_Z)&&e.framebufferTexture2D(e.FRAMEBUFFER,s,l,r.get(o).__webglTexture,u),n.bindFramebuffer(e.FRAMEBUFFER,null)}function se(t,n,r){if(e.bindRenderbuffer(e.RENDERBUFFER,t),n.depthBuffer&&!n.stencilBuffer){let i=s===!0?e.DEPTH_COMPONENT24:e.DEPTH_COMPONENT16;if(r||U(n)){let t=n.depthTexture;t&&t.isDepthTexture&&(t.type===1015?i=e.DEPTH_COMPONENT32F:t.type===1014&&(i=e.DEPTH_COMPONENT24));let r=H(n);U(n)?c.renderbufferStorageMultisampleEXT(e.RENDERBUFFER,r,i,n.width,n.height):e.renderbufferStorageMultisample(e.RENDERBUFFER,r,i,n.width,n.height)}else e.renderbufferStorage(e.RENDERBUFFER,i,n.width,n.height);e.framebufferRenderbuffer(e.FRAMEBUFFER,e.DEPTH_ATTACHMENT,e.RENDERBUFFER,t)}else if(n.depthBuffer&&n.stencilBuffer){let i=H(n);r&&U(n)===!1?e.renderbufferStorageMultisample(e.RENDERBUFFER,i,e.DEPTH24_STENCIL8,n.width,n.height):U(n)?c.renderbufferStorageMultisampleEXT(e.RENDERBUFFER,i,e.DEPTH24_STENCIL8,n.width,n.height):e.renderbufferStorage(e.RENDERBUFFER,e.DEPTH_STENCIL,n.width,n.height),e.framebufferRenderbuffer(e.FRAMEBUFFER,e.DEPTH_STENCIL_ATTACHMENT,e.RENDERBUFFER,t)}else{let t=n.textures;for(let i=0;i<t.length;i++){let o=t[i],s=a.convert(o.format,o.colorSpace),l=a.convert(o.type),u=x(o.internalFormat,s,l,o.colorSpace),d=H(n);r&&U(n)===!1?e.renderbufferStorageMultisample(e.RENDERBUFFER,d,u,n.width,n.height):U(n)?c.renderbufferStorageMultisampleEXT(e.RENDERBUFFER,d,u,n.width,n.height):e.renderbufferStorage(e.RENDERBUFFER,u,n.width,n.height)}}e.bindRenderbuffer(e.RENDERBUFFER,null)}function ce(t,i){if(i&&i.isWebGLCubeRenderTarget)throw Error(`Depth Texture with cube render targets is not supported`);if(n.bindFramebuffer(e.FRAMEBUFFER,t),!(i.depthTexture&&i.depthTexture.isDepthTexture))throw Error(`renderTarget.depthTexture must be an instance of THREE.DepthTexture`);(!r.get(i.depthTexture).__webglTexture||i.depthTexture.image.width!==i.width||i.depthTexture.image.height!==i.height)&&(i.depthTexture.image.width=i.width,i.depthTexture.image.height=i.height,i.depthTexture.needsUpdate=!0),N(i.depthTexture,0);let a=r.get(i.depthTexture).__webglTexture,o=H(i);if(i.depthTexture.format===1026)U(i)?c.framebufferTexture2DMultisampleEXT(e.FRAMEBUFFER,e.DEPTH_ATTACHMENT,e.TEXTURE_2D,a,0,o):e.framebufferTexture2D(e.FRAMEBUFFER,e.DEPTH_ATTACHMENT,e.TEXTURE_2D,a,0);else if(i.depthTexture.format===1027)U(i)?c.framebufferTexture2DMultisampleEXT(e.FRAMEBUFFER,e.DEPTH_STENCIL_ATTACHMENT,e.TEXTURE_2D,a,0,o):e.framebufferTexture2D(e.FRAMEBUFFER,e.DEPTH_STENCIL_ATTACHMENT,e.TEXTURE_2D,a,0);else throw Error(`Unknown depthTexture format`)}function R(t){let i=r.get(t),a=t.isWebGLCubeRenderTarget===!0;if(t.depthTexture&&!i.__autoAllocateDepthBuffer){if(a)throw Error(`target.depthTexture not supported in Cube render targets`);ce(i.__webglFramebuffer,t)}else if(a){i.__webglDepthbuffer=[];for(let r=0;r<6;r++)n.bindFramebuffer(e.FRAMEBUFFER,i.__webglFramebuffer[r]),i.__webglDepthbuffer[r]=e.createRenderbuffer(),se(i.__webglDepthbuffer[r],t,!1)}else n.bindFramebuffer(e.FRAMEBUFFER,i.__webglFramebuffer),i.__webglDepthbuffer=e.createRenderbuffer(),se(i.__webglDepthbuffer,t,!1);n.bindFramebuffer(e.FRAMEBUFFER,null)}function le(t,n,i){let a=r.get(t);n!==void 0&&L(a.__webglFramebuffer,t,t.texture,e.COLOR_ATTACHMENT0,e.TEXTURE_2D,0),i!==void 0&&R(t)}function z(t){let c=t.texture,l=r.get(t),u=r.get(c);t.addEventListener(`dispose`,T);let d=t.textures,f=t.isWebGLCubeRenderTarget===!0,p=d.length>1,m=_(t)||s;if(p||(u.__webglTexture===void 0&&(u.__webglTexture=e.createTexture()),u.__version=c.version,o.memory.textures++),f){l.__webglFramebuffer=[];for(let t=0;t<6;t++)if(s&&c.mipmaps&&c.mipmaps.length>0){l.__webglFramebuffer[t]=[];for(let n=0;n<c.mipmaps.length;n++)l.__webglFramebuffer[t][n]=e.createFramebuffer()}else l.__webglFramebuffer[t]=e.createFramebuffer()}else{if(s&&c.mipmaps&&c.mipmaps.length>0){l.__webglFramebuffer=[];for(let t=0;t<c.mipmaps.length;t++)l.__webglFramebuffer[t]=e.createFramebuffer()}else l.__webglFramebuffer=e.createFramebuffer();if(p)if(i.drawBuffers)for(let t=0,n=d.length;t<n;t++){let n=r.get(d[t]);n.__webglTexture===void 0&&(n.__webglTexture=e.createTexture(),o.memory.textures++)}else console.warn(`THREE.WebGLRenderer: WebGLMultipleRenderTargets can only be used with WebGL2 or WEBGL_draw_buffers extension.`);if(s&&t.samples>0&&U(t)===!1){l.__webglMultisampledFramebuffer=e.createFramebuffer(),l.__webglColorRenderbuffer=[],n.bindFramebuffer(e.FRAMEBUFFER,l.__webglMultisampledFramebuffer);for(let n=0;n<d.length;n++){let r=d[n];l.__webglColorRenderbuffer[n]=e.createRenderbuffer(),e.bindRenderbuffer(e.RENDERBUFFER,l.__webglColorRenderbuffer[n]);let i=a.convert(r.format,r.colorSpace),o=a.convert(r.type),s=x(r.internalFormat,i,o,r.colorSpace,t.isXRRenderTarget===!0),c=H(t);e.renderbufferStorageMultisample(e.RENDERBUFFER,c,s,t.width,t.height),e.framebufferRenderbuffer(e.FRAMEBUFFER,e.COLOR_ATTACHMENT0+n,e.RENDERBUFFER,l.__webglColorRenderbuffer[n])}e.bindRenderbuffer(e.RENDERBUFFER,null),t.depthBuffer&&(l.__webglDepthRenderbuffer=e.createRenderbuffer(),se(l.__webglDepthRenderbuffer,t,!0)),n.bindFramebuffer(e.FRAMEBUFFER,null)}}if(f){n.bindTexture(e.TEXTURE_CUBE_MAP,u.__webglTexture),re(e.TEXTURE_CUBE_MAP,c,m);for(let n=0;n<6;n++)if(s&&c.mipmaps&&c.mipmaps.length>0)for(let r=0;r<c.mipmaps.length;r++)L(l.__webglFramebuffer[n][r],t,c,e.COLOR_ATTACHMENT0,e.TEXTURE_CUBE_MAP_POSITIVE_X+n,r);else L(l.__webglFramebuffer[n],t,c,e.COLOR_ATTACHMENT0,e.TEXTURE_CUBE_MAP_POSITIVE_X+n,0);y(c,m)&&b(e.TEXTURE_CUBE_MAP),n.unbindTexture()}else if(p){for(let i=0,a=d.length;i<a;i++){let a=d[i],o=r.get(a);n.bindTexture(e.TEXTURE_2D,o.__webglTexture),re(e.TEXTURE_2D,a,m),L(l.__webglFramebuffer,t,a,e.COLOR_ATTACHMENT0+i,e.TEXTURE_2D,0),y(a,m)&&b(e.TEXTURE_2D)}n.unbindTexture()}else{let r=e.TEXTURE_2D;if((t.isWebGL3DRenderTarget||t.isWebGLArrayRenderTarget)&&(s?r=t.isWebGL3DRenderTarget?e.TEXTURE_3D:e.TEXTURE_2D_ARRAY:console.error(`THREE.WebGLTextures: THREE.Data3DTexture and THREE.DataArrayTexture only supported with WebGL2.`)),n.bindTexture(r,u.__webglTexture),re(r,c,m),s&&c.mipmaps&&c.mipmaps.length>0)for(let n=0;n<c.mipmaps.length;n++)L(l.__webglFramebuffer[n],t,c,e.COLOR_ATTACHMENT0,r,n);else L(l.__webglFramebuffer,t,c,e.COLOR_ATTACHMENT0,r,0);y(c,m)&&b(r),n.unbindTexture()}t.depthBuffer&&R(t)}function B(t){let i=_(t)||s,a=t.textures;for(let o=0,s=a.length;o<s;o++){let s=a[o];if(y(s,i)){let i=t.isWebGLCubeRenderTarget?e.TEXTURE_CUBE_MAP:e.TEXTURE_2D,a=r.get(s).__webglTexture;n.bindTexture(i,a),b(i),n.unbindTexture()}}}function V(t){if(s&&t.samples>0&&U(t)===!1){let i=t.textures,a=t.width,o=t.height,s=e.COLOR_BUFFER_BIT,c=[],u=t.stencilBuffer?e.DEPTH_STENCIL_ATTACHMENT:e.DEPTH_ATTACHMENT,d=r.get(t),f=i.length>1;if(f)for(let t=0;t<i.length;t++)n.bindFramebuffer(e.FRAMEBUFFER,d.__webglMultisampledFramebuffer),e.framebufferRenderbuffer(e.FRAMEBUFFER,e.COLOR_ATTACHMENT0+t,e.RENDERBUFFER,null),n.bindFramebuffer(e.FRAMEBUFFER,d.__webglFramebuffer),e.framebufferTexture2D(e.DRAW_FRAMEBUFFER,e.COLOR_ATTACHMENT0+t,e.TEXTURE_2D,null,0);n.bindFramebuffer(e.READ_FRAMEBUFFER,d.__webglMultisampledFramebuffer),n.bindFramebuffer(e.DRAW_FRAMEBUFFER,d.__webglFramebuffer);for(let n=0;n<i.length;n++){c.push(e.COLOR_ATTACHMENT0+n),t.depthBuffer&&c.push(u);let p=d.__ignoreDepthValues===void 0?!1:d.__ignoreDepthValues;if(p===!1&&(t.depthBuffer&&(s|=e.DEPTH_BUFFER_BIT),t.stencilBuffer&&(s|=e.STENCIL_BUFFER_BIT)),f&&e.framebufferRenderbuffer(e.READ_FRAMEBUFFER,e.COLOR_ATTACHMENT0,e.RENDERBUFFER,d.__webglColorRenderbuffer[n]),p===!0&&(e.invalidateFramebuffer(e.READ_FRAMEBUFFER,[u]),e.invalidateFramebuffer(e.DRAW_FRAMEBUFFER,[u])),f){let t=r.get(i[n]).__webglTexture;e.framebufferTexture2D(e.DRAW_FRAMEBUFFER,e.COLOR_ATTACHMENT0,e.TEXTURE_2D,t,0)}e.blitFramebuffer(0,0,a,o,0,0,a,o,s,e.NEAREST),l&&e.invalidateFramebuffer(e.READ_FRAMEBUFFER,c)}if(n.bindFramebuffer(e.READ_FRAMEBUFFER,null),n.bindFramebuffer(e.DRAW_FRAMEBUFFER,null),f)for(let t=0;t<i.length;t++){n.bindFramebuffer(e.FRAMEBUFFER,d.__webglMultisampledFramebuffer),e.framebufferRenderbuffer(e.FRAMEBUFFER,e.COLOR_ATTACHMENT0+t,e.RENDERBUFFER,d.__webglColorRenderbuffer[t]);let a=r.get(i[t]).__webglTexture;n.bindFramebuffer(e.FRAMEBUFFER,d.__webglFramebuffer),e.framebufferTexture2D(e.DRAW_FRAMEBUFFER,e.COLOR_ATTACHMENT0+t,e.TEXTURE_2D,a,0)}n.bindFramebuffer(e.DRAW_FRAMEBUFFER,d.__webglMultisampledFramebuffer)}}function H(e){return Math.min(i.maxSamples,e.samples)}function U(e){let n=r.get(e);return s&&e.samples>0&&t.has(`WEBGL_multisampled_render_to_texture`)===!0&&n.__useRenderToTexture!==!1}function W(e){let t=o.render.frame;d.get(e)!==t&&(d.set(e,t),e.update())}function ue(e,n){let r=e.colorSpace,i=e.format,a=e.type;return e.isCompressedTexture===!0||e.isVideoTexture===!0||e.format===1035||r!==`srgb-linear`&&r!==``&&(Hr.getTransfer(r)===`srgb`?s===!1?t.has(`EXT_sRGB`)===!0&&i===1023?(e.format=nr,e.minFilter=On,e.generateMipmaps=!1):n=Kr.sRGBToLinear(n):(i!==1023||a!==1009)&&console.warn(`THREE.WebGLTextures: sRGB encoded textures have to use RGBAFormat and UnsignedByteType.`):console.error(`THREE.WebGLTextures: Unsupported texture color space:`,r)),n}function de(e){return typeof HTMLImageElement<`u`&&e instanceof HTMLImageElement?(u.width=e.naturalWidth||e.width,u.height=e.naturalHeight||e.height):typeof VideoFrame<`u`&&e instanceof VideoFrame?(u.width=e.displayWidth,u.height=e.displayHeight):(u.width=e.width,u.height=e.height),u}this.allocateTextureUnit=j,this.resetTextureUnits=A,this.setTexture2D=N,this.setTexture2DArray=P,this.setTexture3D=F,this.setTextureCube=ee,this.rebindTextures=le,this.setupRenderTarget=z,this.updateRenderTargetMipmap=B,this.updateMultisampleRenderTarget=V,this.setupDepthRenderbuffer=R,this.setupFrameBufferTexture=L,this.useMultisampledRTT=U}function Ol(e,t,n){let r=n.isWebGL2;function i(n,i=``){let a,o=Hr.getTransfer(i);if(n===1009)return e.UNSIGNED_BYTE;if(n===1017)return e.UNSIGNED_SHORT_4_4_4_4;if(n===1018)return e.UNSIGNED_SHORT_5_5_5_1;if(n===1010)return e.BYTE;if(n===1011)return e.SHORT;if(n===1012)return e.UNSIGNED_SHORT;if(n===1013)return e.INT;if(n===1014)return e.UNSIGNED_INT;if(n===1015)return e.FLOAT;if(n===1016)return r?e.HALF_FLOAT:(a=t.get(`OES_texture_half_float`),a===null?null:a.HALF_FLOAT_OES);if(n===1021)return e.ALPHA;if(n===1023)return e.RGBA;if(n===1024)return e.LUMINANCE;if(n===1025)return e.LUMINANCE_ALPHA;if(n===1026)return e.DEPTH_COMPONENT;if(n===1027)return e.DEPTH_STENCIL;if(n===1035)return a=t.get(`EXT_sRGB`),a===null?null:a.SRGB_ALPHA_EXT;if(n===1028)return e.RED;if(n===1029)return e.RED_INTEGER;if(n===1030)return e.RG;if(n===1031)return e.RG_INTEGER;if(n===1033)return e.RGBA_INTEGER;if(n===33776||n===33777||n===33778||n===33779)if(o===`srgb`)if(a=t.get(`WEBGL_compressed_texture_s3tc_srgb`),a!==null){if(n===33776)return a.COMPRESSED_SRGB_S3TC_DXT1_EXT;if(n===33777)return a.COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;if(n===33778)return a.COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;if(n===33779)return a.COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT}else return null;else if(a=t.get(`WEBGL_compressed_texture_s3tc`),a!==null){if(n===33776)return a.COMPRESSED_RGB_S3TC_DXT1_EXT;if(n===33777)return a.COMPRESSED_RGBA_S3TC_DXT1_EXT;if(n===33778)return a.COMPRESSED_RGBA_S3TC_DXT3_EXT;if(n===33779)return a.COMPRESSED_RGBA_S3TC_DXT5_EXT}else return null;if(n===35840||n===35841||n===35842||n===35843)if(a=t.get(`WEBGL_compressed_texture_pvrtc`),a!==null){if(n===35840)return a.COMPRESSED_RGB_PVRTC_4BPPV1_IMG;if(n===35841)return a.COMPRESSED_RGB_PVRTC_2BPPV1_IMG;if(n===35842)return a.COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;if(n===35843)return a.COMPRESSED_RGBA_PVRTC_2BPPV1_IMG}else return null;if(n===36196)return a=t.get(`WEBGL_compressed_texture_etc1`),a===null?null:a.COMPRESSED_RGB_ETC1_WEBGL;if(n===37492||n===37496)if(a=t.get(`WEBGL_compressed_texture_etc`),a!==null){if(n===37492)return o===`srgb`?a.COMPRESSED_SRGB8_ETC2:a.COMPRESSED_RGB8_ETC2;if(n===37496)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:a.COMPRESSED_RGBA8_ETC2_EAC}else return null;if(n===37808||n===37809||n===37810||n===37811||n===37812||n===37813||n===37814||n===37815||n===37816||n===37817||n===37818||n===37819||n===37820||n===37821)if(a=t.get(`WEBGL_compressed_texture_astc`),a!==null){if(n===37808)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:a.COMPRESSED_RGBA_ASTC_4x4_KHR;if(n===37809)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:a.COMPRESSED_RGBA_ASTC_5x4_KHR;if(n===37810)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:a.COMPRESSED_RGBA_ASTC_5x5_KHR;if(n===37811)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:a.COMPRESSED_RGBA_ASTC_6x5_KHR;if(n===37812)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:a.COMPRESSED_RGBA_ASTC_6x6_KHR;if(n===37813)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:a.COMPRESSED_RGBA_ASTC_8x5_KHR;if(n===37814)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:a.COMPRESSED_RGBA_ASTC_8x6_KHR;if(n===37815)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:a.COMPRESSED_RGBA_ASTC_8x8_KHR;if(n===37816)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:a.COMPRESSED_RGBA_ASTC_10x5_KHR;if(n===37817)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:a.COMPRESSED_RGBA_ASTC_10x6_KHR;if(n===37818)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:a.COMPRESSED_RGBA_ASTC_10x8_KHR;if(n===37819)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:a.COMPRESSED_RGBA_ASTC_10x10_KHR;if(n===37820)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:a.COMPRESSED_RGBA_ASTC_12x10_KHR;if(n===37821)return o===`srgb`?a.COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:a.COMPRESSED_RGBA_ASTC_12x12_KHR}else return null;if(n===36492||n===36494||n===36495)if(a=t.get(`EXT_texture_compression_bptc`),a!==null){if(n===36492)return o===`srgb`?a.COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT:a.COMPRESSED_RGBA_BPTC_UNORM_EXT;if(n===36494)return a.COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT;if(n===36495)return a.COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT}else return null;if(n===36283||n===36284||n===36285||n===36286)if(a=t.get(`EXT_texture_compression_rgtc`),a!==null){if(n===36492)return a.COMPRESSED_RED_RGTC1_EXT;if(n===36284)return a.COMPRESSED_SIGNED_RED_RGTC1_EXT;if(n===36285)return a.COMPRESSED_RED_GREEN_RGTC2_EXT;if(n===36286)return a.COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT}else return null;return n===1020?r?e.UNSIGNED_INT_24_8:(a=t.get(`WEBGL_depth_texture`),a===null?null:a.UNSIGNED_INT_24_8_WEBGL):e[n]===void 0?null:e[n]}return{convert:i}}var kl=class extends xo{constructor(e=[]){super(),this.isArrayCamera=!0,this.cameras=e}},Al=class extends oa{constructor(){super(),this.isGroup=!0,this.type=`Group`}},jl={type:`move`},Ml=class{constructor(){this._targetRay=null,this._grip=null,this._hand=null}getHandSpace(){return this._hand===null&&(this._hand=new Al,this._hand.matrixAutoUpdate=!1,this._hand.visible=!1,this._hand.joints={},this._hand.inputState={pinching:!1}),this._hand}getTargetRaySpace(){return this._targetRay===null&&(this._targetRay=new Al,this._targetRay.matrixAutoUpdate=!1,this._targetRay.visible=!1,this._targetRay.hasLinearVelocity=!1,this._targetRay.linearVelocity=new Z,this._targetRay.hasAngularVelocity=!1,this._targetRay.angularVelocity=new Z),this._targetRay}getGripSpace(){return this._grip===null&&(this._grip=new Al,this._grip.matrixAutoUpdate=!1,this._grip.visible=!1,this._grip.hasLinearVelocity=!1,this._grip.linearVelocity=new Z,this._grip.hasAngularVelocity=!1,this._grip.angularVelocity=new Z),this._grip}dispatchEvent(e){return this._targetRay!==null&&this._targetRay.dispatchEvent(e),this._grip!==null&&this._grip.dispatchEvent(e),this._hand!==null&&this._hand.dispatchEvent(e),this}connect(e){if(e&&e.hand){let t=this._hand;if(t)for(let n of e.hand.values())this._getHandJoint(t,n)}return this.dispatchEvent({type:`connected`,data:e}),this}disconnect(e){return this.dispatchEvent({type:`disconnected`,data:e}),this._targetRay!==null&&(this._targetRay.visible=!1),this._grip!==null&&(this._grip.visible=!1),this._hand!==null&&(this._hand.visible=!1),this}update(e,t,n){let r=null,i=null,a=null,o=this._targetRay,s=this._grip,c=this._hand;if(e&&t.session.visibilityState!==`visible-blurred`){if(c&&e.hand){a=!0;for(let r of e.hand.values()){let e=t.getJointPose(r,n),i=this._getHandJoint(c,r);e!==null&&(i.matrix.fromArray(e.transform.matrix),i.matrix.decompose(i.position,i.rotation,i.scale),i.matrixWorldNeedsUpdate=!0,i.jointRadius=e.radius),i.visible=e!==null}let r=c.joints[`index-finger-tip`],i=c.joints[`thumb-tip`],o=r.position.distanceTo(i.position),s=.02,l=.005;c.inputState.pinching&&o>s+l?(c.inputState.pinching=!1,this.dispatchEvent({type:`pinchend`,handedness:e.handedness,target:this})):!c.inputState.pinching&&o<=s-l&&(c.inputState.pinching=!0,this.dispatchEvent({type:`pinchstart`,handedness:e.handedness,target:this}))}else s!==null&&e.gripSpace&&(i=t.getPose(e.gripSpace,n),i!==null&&(s.matrix.fromArray(i.transform.matrix),s.matrix.decompose(s.position,s.rotation,s.scale),s.matrixWorldNeedsUpdate=!0,i.linearVelocity?(s.hasLinearVelocity=!0,s.linearVelocity.copy(i.linearVelocity)):s.hasLinearVelocity=!1,i.angularVelocity?(s.hasAngularVelocity=!0,s.angularVelocity.copy(i.angularVelocity)):s.hasAngularVelocity=!1));o!==null&&(r=t.getPose(e.targetRaySpace,n),r===null&&i!==null&&(r=i),r!==null&&(o.matrix.fromArray(r.transform.matrix),o.matrix.decompose(o.position,o.rotation,o.scale),o.matrixWorldNeedsUpdate=!0,r.linearVelocity?(o.hasLinearVelocity=!0,o.linearVelocity.copy(r.linearVelocity)):o.hasLinearVelocity=!1,r.angularVelocity?(o.hasAngularVelocity=!0,o.angularVelocity.copy(r.angularVelocity)):o.hasAngularVelocity=!1,this.dispatchEvent(jl)))}return o!==null&&(o.visible=r!==null),s!==null&&(s.visible=i!==null),c!==null&&(c.visible=a!==null),this}_getHandJoint(e,t){if(e.joints[t.jointName]===void 0){let n=new Al;n.matrixAutoUpdate=!1,n.visible=!1,e.joints[t.jointName]=n,e.add(n)}return e.joints[t.jointName]}},Nl=`
void main() {

	gl_Position = vec4( position, 1.0 );

}`,Pl=`
uniform sampler2DArray depthColor;
uniform float depthWidth;
uniform float depthHeight;

void main() {

	vec2 coord = vec2( gl_FragCoord.x / depthWidth, gl_FragCoord.y / depthHeight );

	if ( coord.x >= 1.0 ) {

		gl_FragDepthEXT = texture( depthColor, vec3( coord.x - 1.0, coord.y, 1 ) ).r;

	} else {

		gl_FragDepthEXT = texture( depthColor, vec3( coord.x, coord.y, 0 ) ).r;

	}

}`,Fl=class{constructor(){this.texture=null,this.mesh=null,this.depthNear=0,this.depthFar=0}init(e,t,n){if(this.texture===null){let r=new Zr,i=e.properties.get(r);i.__webglTexture=t.texture,(t.depthNear!=n.depthNear||t.depthFar!=n.depthFar)&&(this.depthNear=t.depthNear,this.depthFar=t.depthFar),this.texture=r}}render(e,t){if(this.texture!==null){if(this.mesh===null){let e=t.cameras[0].viewport,n=new go({extensions:{fragDepth:!0},vertexShader:Nl,fragmentShader:Pl,uniforms:{depthColor:{value:this.texture},depthWidth:{value:e.z},depthHeight:{value:e.w}}});this.mesh=new io(new Io(20,20),n)}e.render(this.mesh,t)}}reset(){this.texture=null,this.mesh=null}},Il=class extends ir{constructor(e,t){super();let n=this,r=null,i=1,a=null,o=`local-floor`,s=1,c=null,l=null,u=null,d=null,f=null,p=null,m=new Fl,h=t.getContextAttributes(),g=null,_=null,v=[],y=[],b=new Y,x=null,S=new xo;S.layers.enable(1),S.viewport=new Qr;let C=new xo;C.layers.enable(2),C.viewport=new Qr;let w=[S,C],T=new kl;T.layers.enable(1),T.layers.enable(2);let E=null,D=null;this.cameraAutoUpdate=!0,this.enabled=!1,this.isPresenting=!1,this.getController=function(e){let t=v[e];return t===void 0&&(t=new Ml,v[e]=t),t.getTargetRaySpace()},this.getControllerGrip=function(e){let t=v[e];return t===void 0&&(t=new Ml,v[e]=t),t.getGripSpace()},this.getHand=function(e){let t=v[e];return t===void 0&&(t=new Ml,v[e]=t),t.getHandSpace()};function O(e){let t=y.indexOf(e.inputSource);if(t===-1)return;let n=v[t];n!==void 0&&(n.update(e.inputSource,e.frame,c||a),n.dispatchEvent({type:e.type,data:e.inputSource}))}function k(){r.removeEventListener(`select`,O),r.removeEventListener(`selectstart`,O),r.removeEventListener(`selectend`,O),r.removeEventListener(`squeeze`,O),r.removeEventListener(`squeezestart`,O),r.removeEventListener(`squeezeend`,O),r.removeEventListener(`end`,k),r.removeEventListener(`inputsourceschange`,A);for(let e=0;e<v.length;e++){let t=y[e];t!==null&&(y[e]=null,v[e].disconnect(t))}E=null,D=null,m.reset(),e.setRenderTarget(g),f=null,d=null,u=null,r=null,_=null,te.stop(),n.isPresenting=!1,e.setPixelRatio(x),e.setSize(b.width,b.height,!1),n.dispatchEvent({type:`sessionend`})}this.setFramebufferScaleFactor=function(e){i=e,n.isPresenting===!0&&console.warn(`THREE.WebXRManager: Cannot change framebuffer scale while presenting.`)},this.setReferenceSpaceType=function(e){o=e,n.isPresenting===!0&&console.warn(`THREE.WebXRManager: Cannot change reference space type while presenting.`)},this.getReferenceSpace=function(){return c||a},this.setReferenceSpace=function(e){c=e},this.getBaseLayer=function(){return d===null?f:d},this.getBinding=function(){return u},this.getFrame=function(){return p},this.getSession=function(){return r},this.setSession=async function(l){if(r=l,r!==null){if(g=e.getRenderTarget(),r.addEventListener(`select`,O),r.addEventListener(`selectstart`,O),r.addEventListener(`selectend`,O),r.addEventListener(`squeeze`,O),r.addEventListener(`squeezestart`,O),r.addEventListener(`squeezeend`,O),r.addEventListener(`end`,k),r.addEventListener(`inputsourceschange`,A),h.xrCompatible!==!0&&await t.makeXRCompatible(),x=e.getPixelRatio(),e.getSize(b),r.renderState.layers===void 0||e.capabilities.isWebGL2===!1){let n={antialias:r.renderState.layers===void 0?h.antialias:!0,alpha:!0,depth:h.depth,stencil:h.stencil,framebufferScaleFactor:i};f=new XRWebGLLayer(r,t,n),r.updateRenderState({baseLayer:f}),e.setPixelRatio(1),e.setSize(f.framebufferWidth,f.framebufferHeight,!1),_=new ei(f.framebufferWidth,f.framebufferHeight,{format:In,type:jn,colorSpace:e.outputColorSpace,stencilBuffer:h.stencil})}else{let n=null,a=null,o=null;h.depth&&(o=h.stencil?t.DEPTH24_STENCIL8:t.DEPTH_COMPONENT24,n=h.stencil?Rn:Ln,a=h.stencil?Fn:Mn);let s={colorFormat:t.RGBA8,depthFormat:o,scaleFactor:i};u=new XRWebGLBinding(r,t),d=u.createProjectionLayer(s),r.updateRenderState({layers:[d]}),e.setPixelRatio(1),e.setSize(d.textureWidth,d.textureHeight,!1),_=new ei(d.textureWidth,d.textureHeight,{format:In,type:jn,depthTexture:new Ss(d.textureWidth,d.textureHeight,a,void 0,void 0,void 0,void 0,void 0,void 0,n),stencilBuffer:h.stencil,colorSpace:e.outputColorSpace,samples:h.antialias?4:0});let c=e.properties.get(_);c.__ignoreDepthValues=d.ignoreDepthValues}_.isXRRenderTarget=!0,this.setFoveation(s),c=null,a=await r.requestReferenceSpace(o),te.setContext(r),te.start(),n.isPresenting=!0,n.dispatchEvent({type:`sessionstart`})}},this.getEnvironmentBlendMode=function(){if(r!==null)return r.environmentBlendMode};function A(e){for(let t=0;t<e.removed.length;t++){let n=e.removed[t],r=y.indexOf(n);r>=0&&(y[r]=null,v[r].disconnect(n))}for(let t=0;t<e.added.length;t++){let n=e.added[t],r=y.indexOf(n);if(r===-1){for(let e=0;e<v.length;e++)if(e>=y.length){y.push(n),r=e;break}else if(y[e]===null){y[e]=n,r=e;break}if(r===-1)break}let i=v[r];i&&i.connect(n)}}let j=new Z,M=new Z;function N(e,t,n){j.setFromMatrixPosition(t.matrixWorld),M.setFromMatrixPosition(n.matrixWorld);let r=j.distanceTo(M),i=t.projectionMatrix.elements,a=n.projectionMatrix.elements,o=i[14]/(i[10]-1),s=i[14]/(i[10]+1),c=(i[9]+1)/i[5],l=(i[9]-1)/i[5],u=(i[8]-1)/i[0],d=(a[8]+1)/a[0],f=o*u,p=o*d,m=r/(-u+d),h=m*-u;t.matrixWorld.decompose(e.position,e.quaternion,e.scale),e.translateX(h),e.translateZ(m),e.matrixWorld.compose(e.position,e.quaternion,e.scale),e.matrixWorldInverse.copy(e.matrixWorld).invert();let g=o+m,_=s+m,v=f-h,y=p+(r-h),b=c*s/_*g,x=l*s/_*g;e.projectionMatrix.makePerspective(v,y,b,x,g,_),e.projectionMatrixInverse.copy(e.projectionMatrix).invert()}function P(e,t){t===null?e.matrixWorld.copy(e.matrix):e.matrixWorld.multiplyMatrices(t.matrixWorld,e.matrix),e.matrixWorldInverse.copy(e.matrixWorld).invert()}this.updateCamera=function(e){if(r===null)return;m.texture!==null&&(e.near=m.depthNear,e.far=m.depthFar),T.near=C.near=S.near=e.near,T.far=C.far=S.far=e.far,(E!==T.near||D!==T.far)&&(r.updateRenderState({depthNear:T.near,depthFar:T.far}),E=T.near,D=T.far,S.near=E,S.far=D,C.near=E,C.far=D,S.updateProjectionMatrix(),C.updateProjectionMatrix(),e.updateProjectionMatrix());let t=e.parent,n=T.cameras;P(T,t);for(let e=0;e<n.length;e++)P(n[e],t);n.length===2?N(T,S,C):T.projectionMatrix.copy(S.projectionMatrix),F(e,T,t)};function F(e,t,n){n===null?e.matrix.copy(t.matrixWorld):(e.matrix.copy(n.matrixWorld),e.matrix.invert(),e.matrix.multiply(t.matrixWorld)),e.matrix.decompose(e.position,e.quaternion,e.scale),e.updateMatrixWorld(!0),e.projectionMatrix.copy(t.projectionMatrix),e.projectionMatrixInverse.copy(t.projectionMatrixInverse),e.isPerspectiveCamera&&(e.fov=cr*2*Math.atan(1/e.projectionMatrix.elements[5]),e.zoom=1)}this.getCamera=function(){return T},this.getFoveation=function(){if(!(d===null&&f===null))return s},this.setFoveation=function(e){s=e,d!==null&&(d.fixedFoveation=e),f!==null&&f.fixedFoveation!==void 0&&(f.fixedFoveation=e)},this.hasDepthSensing=function(){return m.texture!==null};let ee=null;function I(t,i){if(l=i.getViewerPose(c||a),p=i,l!==null){let t=l.views;f!==null&&(e.setRenderTargetFramebuffer(_,f.framebuffer),e.setRenderTarget(_));let n=!1;t.length!==T.cameras.length&&(T.cameras.length=0,n=!0);for(let r=0;r<t.length;r++){let i=t[r],a=null;if(f!==null)a=f.getViewport(i);else{let t=u.getViewSubImage(d,i);a=t.viewport,r===0&&(e.setRenderTargetTextures(_,t.colorTexture,d.ignoreDepthValues?void 0:t.depthStencilTexture),e.setRenderTarget(_))}let o=w[r];o===void 0&&(o=new xo,o.layers.enable(r),o.viewport=new Qr,w[r]=o),o.matrix.fromArray(i.transform.matrix),o.matrix.decompose(o.position,o.quaternion,o.scale),o.projectionMatrix.fromArray(i.projectionMatrix),o.projectionMatrixInverse.copy(o.projectionMatrix).invert(),o.viewport.set(a.x,a.y,a.width,a.height),r===0&&(T.matrix.copy(o.matrix),T.matrix.decompose(T.position,T.quaternion,T.scale)),n===!0&&T.cameras.push(o)}let i=r.enabledFeatures;if(i&&i.includes(`depth-sensing`)){let n=u.getDepthInformation(t[0]);n&&n.isValid&&n.texture&&m.init(e,n,r.renderState)}}for(let e=0;e<v.length;e++){let t=y[e],n=v[e];t!==null&&n!==void 0&&n.update(t,i,c||a)}m.render(e,T),ee&&ee(t,i),i.detectedPlanes&&n.dispatchEvent({type:`planesdetected`,data:i}),p=null}let te=new Po;te.setAnimationLoop(I),this.setAnimationLoop=function(e){ee=e},this.dispose=function(){}}},Ll=new Ui,Rl=new Ni;function zl(e,t){function n(e,t){e.matrixAutoUpdate===!0&&e.updateMatrix(),t.value.copy(e.matrix)}function r(t,n){n.color.getRGB(t.fogColor.value,fo(e)),n.isFog?(t.fogNear.value=n.near,t.fogFar.value=n.far):n.isFogExp2&&(t.fogDensity.value=n.density)}function i(e,t,n,r,i){t.isMeshBasicMaterial||t.isMeshLambertMaterial?a(e,t):t.isMeshToonMaterial?(a(e,t),d(e,t)):t.isMeshPhongMaterial?(a(e,t),u(e,t)):t.isMeshStandardMaterial?(a(e,t),f(e,t),t.isMeshPhysicalMaterial&&p(e,t,i)):t.isMeshMatcapMaterial?(a(e,t),m(e,t)):t.isMeshDepthMaterial?a(e,t):t.isMeshDistanceMaterial?(a(e,t),h(e,t)):t.isMeshNormalMaterial?a(e,t):t.isLineBasicMaterial?(o(e,t),t.isLineDashedMaterial&&s(e,t)):t.isPointsMaterial?c(e,t,n,r):t.isSpriteMaterial?l(e,t):t.isShadowMaterial?(e.color.value.copy(t.color),e.opacity.value=t.opacity):t.isShaderMaterial&&(t.uniformsNeedUpdate=!1)}function a(r,i){r.opacity.value=i.opacity,i.color&&r.diffuse.value.copy(i.color),i.emissive&&r.emissive.value.copy(i.emissive).multiplyScalar(i.emissiveIntensity),i.map&&(r.map.value=i.map,n(i.map,r.mapTransform)),i.alphaMap&&(r.alphaMap.value=i.alphaMap,n(i.alphaMap,r.alphaMapTransform)),i.bumpMap&&(r.bumpMap.value=i.bumpMap,n(i.bumpMap,r.bumpMapTransform),r.bumpScale.value=i.bumpScale,i.side===1&&(r.bumpScale.value*=-1)),i.normalMap&&(r.normalMap.value=i.normalMap,n(i.normalMap,r.normalMapTransform),r.normalScale.value.copy(i.normalScale),i.side===1&&r.normalScale.value.negate()),i.displacementMap&&(r.displacementMap.value=i.displacementMap,n(i.displacementMap,r.displacementMapTransform),r.displacementScale.value=i.displacementScale,r.displacementBias.value=i.displacementBias),i.emissiveMap&&(r.emissiveMap.value=i.emissiveMap,n(i.emissiveMap,r.emissiveMapTransform)),i.specularMap&&(r.specularMap.value=i.specularMap,n(i.specularMap,r.specularMapTransform)),i.alphaTest>0&&(r.alphaTest.value=i.alphaTest);let a=t.get(i),o=a.envMap,s=a.envMapRotation;if(o&&(r.envMap.value=o,Ll.copy(s),Ll.x*=-1,Ll.y*=-1,Ll.z*=-1,o.isCubeTexture&&o.isRenderTargetTexture===!1&&(Ll.y*=-1,Ll.z*=-1),r.envMapRotation.value.setFromMatrix4(Rl.makeRotationFromEuler(Ll)),r.flipEnvMap.value=o.isCubeTexture&&o.isRenderTargetTexture===!1?-1:1,r.reflectivity.value=i.reflectivity,r.ior.value=i.ior,r.refractionRatio.value=i.refractionRatio),i.lightMap){r.lightMap.value=i.lightMap;let t=e._useLegacyLights===!0?Math.PI:1;r.lightMapIntensity.value=i.lightMapIntensity*t,n(i.lightMap,r.lightMapTransform)}i.aoMap&&(r.aoMap.value=i.aoMap,r.aoMapIntensity.value=i.aoMapIntensity,n(i.aoMap,r.aoMapTransform))}function o(e,t){e.diffuse.value.copy(t.color),e.opacity.value=t.opacity,t.map&&(e.map.value=t.map,n(t.map,e.mapTransform))}function s(e,t){e.dashSize.value=t.dashSize,e.totalSize.value=t.dashSize+t.gapSize,e.scale.value=t.scale}function c(e,t,r,i){e.diffuse.value.copy(t.color),e.opacity.value=t.opacity,e.size.value=t.size*r,e.scale.value=i*.5,t.map&&(e.map.value=t.map,n(t.map,e.uvTransform)),t.alphaMap&&(e.alphaMap.value=t.alphaMap,n(t.alphaMap,e.alphaMapTransform)),t.alphaTest>0&&(e.alphaTest.value=t.alphaTest)}function l(e,t){e.diffuse.value.copy(t.color),e.opacity.value=t.opacity,e.rotation.value=t.rotation,t.map&&(e.map.value=t.map,n(t.map,e.mapTransform)),t.alphaMap&&(e.alphaMap.value=t.alphaMap,n(t.alphaMap,e.alphaMapTransform)),t.alphaTest>0&&(e.alphaTest.value=t.alphaTest)}function u(e,t){e.specular.value.copy(t.specular),e.shininess.value=Math.max(t.shininess,1e-4)}function d(e,t){t.gradientMap&&(e.gradientMap.value=t.gradientMap)}function f(e,r){e.metalness.value=r.metalness,r.metalnessMap&&(e.metalnessMap.value=r.metalnessMap,n(r.metalnessMap,e.metalnessMapTransform)),e.roughness.value=r.roughness,r.roughnessMap&&(e.roughnessMap.value=r.roughnessMap,n(r.roughnessMap,e.roughnessMapTransform)),t.get(r).envMap&&(e.envMapIntensity.value=r.envMapIntensity)}function p(e,t,r){e.ior.value=t.ior,t.sheen>0&&(e.sheenColor.value.copy(t.sheenColor).multiplyScalar(t.sheen),e.sheenRoughness.value=t.sheenRoughness,t.sheenColorMap&&(e.sheenColorMap.value=t.sheenColorMap,n(t.sheenColorMap,e.sheenColorMapTransform)),t.sheenRoughnessMap&&(e.sheenRoughnessMap.value=t.sheenRoughnessMap,n(t.sheenRoughnessMap,e.sheenRoughnessMapTransform))),t.clearcoat>0&&(e.clearcoat.value=t.clearcoat,e.clearcoatRoughness.value=t.clearcoatRoughness,t.clearcoatMap&&(e.clearcoatMap.value=t.clearcoatMap,n(t.clearcoatMap,e.clearcoatMapTransform)),t.clearcoatRoughnessMap&&(e.clearcoatRoughnessMap.value=t.clearcoatRoughnessMap,n(t.clearcoatRoughnessMap,e.clearcoatRoughnessMapTransform)),t.clearcoatNormalMap&&(e.clearcoatNormalMap.value=t.clearcoatNormalMap,n(t.clearcoatNormalMap,e.clearcoatNormalMapTransform),e.clearcoatNormalScale.value.copy(t.clearcoatNormalScale),t.side===1&&e.clearcoatNormalScale.value.negate())),t.iridescence>0&&(e.iridescence.value=t.iridescence,e.iridescenceIOR.value=t.iridescenceIOR,e.iridescenceThicknessMinimum.value=t.iridescenceThicknessRange[0],e.iridescenceThicknessMaximum.value=t.iridescenceThicknessRange[1],t.iridescenceMap&&(e.iridescenceMap.value=t.iridescenceMap,n(t.iridescenceMap,e.iridescenceMapTransform)),t.iridescenceThicknessMap&&(e.iridescenceThicknessMap.value=t.iridescenceThicknessMap,n(t.iridescenceThicknessMap,e.iridescenceThicknessMapTransform))),t.transmission>0&&(e.transmission.value=t.transmission,e.transmissionSamplerMap.value=r.texture,e.transmissionSamplerSize.value.set(r.width,r.height),t.transmissionMap&&(e.transmissionMap.value=t.transmissionMap,n(t.transmissionMap,e.transmissionMapTransform)),e.thickness.value=t.thickness,t.thicknessMap&&(e.thicknessMap.value=t.thicknessMap,n(t.thicknessMap,e.thicknessMapTransform)),e.attenuationDistance.value=t.attenuationDistance,e.attenuationColor.value.copy(t.attenuationColor)),t.anisotropy>0&&(e.anisotropyVector.value.set(t.anisotropy*Math.cos(t.anisotropyRotation),t.anisotropy*Math.sin(t.anisotropyRotation)),t.anisotropyMap&&(e.anisotropyMap.value=t.anisotropyMap,n(t.anisotropyMap,e.anisotropyMapTransform))),e.specularIntensity.value=t.specularIntensity,e.specularColor.value.copy(t.specularColor),t.specularColorMap&&(e.specularColorMap.value=t.specularColorMap,n(t.specularColorMap,e.specularColorMapTransform)),t.specularIntensityMap&&(e.specularIntensityMap.value=t.specularIntensityMap,n(t.specularIntensityMap,e.specularIntensityMapTransform))}function m(e,t){t.matcap&&(e.matcap.value=t.matcap)}function h(e,n){let r=t.get(n).light;e.referencePosition.value.setFromMatrixPosition(r.matrixWorld),e.nearDistance.value=r.shadow.camera.near,e.farDistance.value=r.shadow.camera.far}return{refreshFogUniforms:r,refreshMaterialUniforms:i}}function Bl(e,t,n,r){let i={},a={},o=[],s=n.isWebGL2?e.getParameter(e.MAX_UNIFORM_BUFFER_BINDINGS):0;function c(e,t){let n=t.program;r.uniformBlockBinding(e,n)}function l(e,n){let o=i[e.id];o===void 0&&(m(e),o=u(e),i[e.id]=o,e.addEventListener(`dispose`,g));let s=n.program;r.updateUBOMapping(e,s);let c=t.render.frame;a[e.id]!==c&&(f(e),a[e.id]=c)}function u(t){let n=d();t.__bindingPointIndex=n;let r=e.createBuffer(),i=t.__size,a=t.usage;return e.bindBuffer(e.UNIFORM_BUFFER,r),e.bufferData(e.UNIFORM_BUFFER,i,a),e.bindBuffer(e.UNIFORM_BUFFER,null),e.bindBufferBase(e.UNIFORM_BUFFER,n,r),r}function d(){for(let e=0;e<s;e++)if(o.indexOf(e)===-1)return o.push(e),e;return console.error(`THREE.WebGLRenderer: Maximum number of simultaneously usable uniforms groups reached.`),0}function f(t){let n=i[t.id],r=t.uniforms,a=t.__cache;e.bindBuffer(e.UNIFORM_BUFFER,n);for(let t=0,n=r.length;t<n;t++){let n=Array.isArray(r[t])?r[t]:[r[t]];for(let r=0,i=n.length;r<i;r++){let i=n[r];if(p(i,t,r,a)===!0){let t=i.__offset,n=Array.isArray(i.value)?i.value:[i.value],r=0;for(let a=0;a<n.length;a++){let o=n[a],s=h(o);typeof o==`number`||typeof o==`boolean`?(i.__data[0]=o,e.bufferSubData(e.UNIFORM_BUFFER,t+r,i.__data)):o.isMatrix3?(i.__data[0]=o.elements[0],i.__data[1]=o.elements[1],i.__data[2]=o.elements[2],i.__data[3]=0,i.__data[4]=o.elements[3],i.__data[5]=o.elements[4],i.__data[6]=o.elements[5],i.__data[7]=0,i.__data[8]=o.elements[6],i.__data[9]=o.elements[7],i.__data[10]=o.elements[8],i.__data[11]=0):(o.toArray(i.__data,r),r+=s.storage/Float32Array.BYTES_PER_ELEMENT)}e.bufferSubData(e.UNIFORM_BUFFER,t,i.__data)}}}e.bindBuffer(e.UNIFORM_BUFFER,null)}function p(e,t,n,r){let i=e.value,a=t+`_`+n;if(r[a]===void 0)return typeof i==`number`||typeof i==`boolean`?r[a]=i:r[a]=i.clone(),!0;{let e=r[a];if(typeof i==`number`||typeof i==`boolean`){if(e!==i)return r[a]=i,!0}else if(e.equals(i)===!1)return e.copy(i),!0}return!1}function m(e){let t=e.uniforms,n=0;for(let e=0,r=t.length;e<r;e++){let r=Array.isArray(t[e])?t[e]:[t[e]];for(let e=0,t=r.length;e<t;e++){let t=r[e],i=Array.isArray(t.value)?t.value:[t.value];for(let e=0,r=i.length;e<r;e++){let r=i[e],a=h(r),o=n%16;o!==0&&16-o<a.boundary&&(n+=16-o),t.__data=new Float32Array(a.storage/Float32Array.BYTES_PER_ELEMENT),t.__offset=n,n+=a.storage}}}let r=n%16;return r>0&&(n+=16-r),e.__size=n,e.__cache={},this}function h(e){let t={boundary:0,storage:0};return typeof e==`number`||typeof e==`boolean`?(t.boundary=4,t.storage=4):e.isVector2?(t.boundary=8,t.storage=8):e.isVector3||e.isColor?(t.boundary=16,t.storage=12):e.isVector4?(t.boundary=16,t.storage=16):e.isMatrix3?(t.boundary=48,t.storage=48):e.isMatrix4?(t.boundary=64,t.storage=64):e.isTexture?console.warn(`THREE.WebGLRenderer: Texture samplers can not be part of an uniforms group.`):console.warn(`THREE.WebGLRenderer: Unsupported uniform value type.`,e),t}function g(t){let n=t.target;n.removeEventListener(`dispose`,g);let r=o.indexOf(n.__bindingPointIndex);o.splice(r,1),e.deleteBuffer(i[n.id]),delete i[n.id],delete a[n.id]}function _(){for(let t in i)e.deleteBuffer(i[t]);o=[],i={},a={}}return{bind:c,update:l,dispose:_}}var Vl=class{constructor(e={}){let{canvas:t=Fr(),context:n=null,depth:r=!0,stencil:i=!0,alpha:a=!1,antialias:o=!1,premultipliedAlpha:s=!0,preserveDrawingBuffer:c=!1,powerPreference:l=`default`,failIfMajorPerformanceCaveat:u=!1}=e;this.isWebGLRenderer=!0;let d;d=n===null?a:n.getContextAttributes().alpha;let f=new Uint32Array(4),p=new Int32Array(4),m=null,h=null,g=[],_=[];this.domElement=t,this.debug={checkShaderErrors:!0,onShaderError:null},this.autoClear=!0,this.autoClearColor=!0,this.autoClearDepth=!0,this.autoClearStencil=!0,this.sortObjects=!0,this.clippingPlanes=[],this.localClippingEnabled=!1,this._outputColorSpace=qn,this._useLegacyLights=!1,this.toneMapping=0,this.toneMappingExposure=1;let v=this,y=!1,b=0,x=0,S=null,C=-1,w=null,T=new Qr,E=new Qr,D=null,O=new Sa(0),k=0,A=t.width,j=t.height,M=1,N=null,P=null,F=new Qr(0,0,A,j),ee=new Qr(0,0,A,j),I=!1,te=new No,ne=!1,re=!1,ie=null,ae=new Ni,oe=new Y,L=new Z,se={background:null,fog:null,environment:null,overrideMaterial:null,isScene:!0};function ce(){return S===null?M:1}let R=n;function le(e,n){for(let r=0;r<e.length;r++){let i=e[r],a=t.getContext(i,n);if(a!==null)return a}return null}try{let e={alpha:!0,depth:r,stencil:i,antialias:o,premultipliedAlpha:s,preserveDrawingBuffer:c,powerPreference:l,failIfMajorPerformanceCaveat:u};if(`setAttribute`in t&&t.setAttribute(`data-engine`,`three.js r162`),t.addEventListener(`webglcontextlost`,ke,!1),t.addEventListener(`webglcontextrestored`,Ae,!1),t.addEventListener(`webglcontextcreationerror`,je,!1),R===null){let t=[`webgl2`,`webgl`,`experimental-webgl`];if(v.isWebGL1Renderer===!0&&t.shift(),R=le(t,e),R===null)throw le(t)?Error(`Error creating WebGL context with your selected attributes.`):Error(`Error creating WebGL context.`)}typeof WebGLRenderingContext<`u`&&R instanceof WebGLRenderingContext&&console.warn(`THREE.WebGLRenderer: WebGL 1 support was deprecated in r153 and will be removed in r163.`),R.getShaderPrecisionFormat===void 0&&(R.getShaderPrecisionFormat=function(){return{rangeMin:1,rangeMax:1,precision:1}})}catch(e){throw console.error(`THREE.WebGLRenderer: `+e.message),e}let z,B,V,H,U,W,ue,de,fe,pe,me,he,ge,_e,ve,ye,be,xe,Se,G,Ce,we,Te,Ee;function De(){z=new ms(R),B=new Wo(R,z,e),z.init(B),we=new Ol(R,z,B),V=new El(R,z,B),H=new _s(R),U=new ll,W=new Dl(R,z,V,U,B,we,H),ue=new Ko(v),de=new ps(v),fe=new Fo(R,B),Te=new Ho(R,z,fe,B),pe=new hs(R,fe,H,Te),me=new xs(R,pe,fe,H),Se=new bs(R,B,W),ye=new Go(U),he=new cl(v,ue,de,z,B,Te,ye),ge=new zl(v,U),_e=new pl,ve=new bl(z,B),xe=new Vo(v,ue,de,V,me,d,s),be=new Tl(v,me,B),Ee=new Bl(R,H,B,V),G=new Uo(R,z,H,B),Ce=new gs(R,z,H,B),H.programs=he.programs,v.capabilities=B,v.extensions=z,v.properties=U,v.renderLists=_e,v.shadowMap=be,v.state=V,v.info=H}De();let Oe=new Il(v,R);this.xr=Oe,this.getContext=function(){return R},this.getContextAttributes=function(){return R.getContextAttributes()},this.forceContextLoss=function(){let e=z.get(`WEBGL_lose_context`);e&&e.loseContext()},this.forceContextRestore=function(){let e=z.get(`WEBGL_lose_context`);e&&e.restoreContext()},this.getPixelRatio=function(){return M},this.setPixelRatio=function(e){e!==void 0&&(M=e,this.setSize(A,j,!1))},this.getSize=function(e){return e.set(A,j)},this.setSize=function(e,n,r=!0){if(Oe.isPresenting){console.warn(`THREE.WebGLRenderer: Can't change size while VR device is presenting.`);return}A=e,j=n,t.width=Math.floor(e*M),t.height=Math.floor(n*M),r===!0&&(t.style.width=e+`px`,t.style.height=n+`px`),this.setViewport(0,0,e,n)},this.getDrawingBufferSize=function(e){return e.set(A*M,j*M).floor()},this.setDrawingBufferSize=function(e,n,r){A=e,j=n,M=r,t.width=Math.floor(e*r),t.height=Math.floor(n*r),this.setViewport(0,0,e,n)},this.getCurrentViewport=function(e){return e.copy(T)},this.getViewport=function(e){return e.copy(F)},this.setViewport=function(e,t,n,r){e.isVector4?F.set(e.x,e.y,e.z,e.w):F.set(e,t,n,r),V.viewport(T.copy(F).multiplyScalar(M).round())},this.getScissor=function(e){return e.copy(ee)},this.setScissor=function(e,t,n,r){e.isVector4?ee.set(e.x,e.y,e.z,e.w):ee.set(e,t,n,r),V.scissor(E.copy(ee).multiplyScalar(M).round())},this.getScissorTest=function(){return I},this.setScissorTest=function(e){V.setScissorTest(I=e)},this.setOpaqueSort=function(e){N=e},this.setTransparentSort=function(e){P=e},this.getClearColor=function(e){return e.copy(xe.getClearColor())},this.setClearColor=function(){xe.setClearColor.apply(xe,arguments)},this.getClearAlpha=function(){return xe.getClearAlpha()},this.setClearAlpha=function(){xe.setClearAlpha.apply(xe,arguments)},this.clear=function(e=!0,t=!0,n=!0){let r=0;if(e){let e=!1;if(S!==null){let t=S.texture.format;e=t===1033||t===1031||t===1029}if(e){let e=S.texture.type,t=e===1009||e===1014||e===1012||e===1020||e===1017||e===1018,n=xe.getClearColor(),r=xe.getClearAlpha(),i=n.r,a=n.g,o=n.b;t?(f[0]=i,f[1]=a,f[2]=o,f[3]=r,R.clearBufferuiv(R.COLOR,0,f)):(p[0]=i,p[1]=a,p[2]=o,p[3]=r,R.clearBufferiv(R.COLOR,0,p))}else r|=R.COLOR_BUFFER_BIT}t&&(r|=R.DEPTH_BUFFER_BIT),n&&(r|=R.STENCIL_BUFFER_BIT,this.state.buffers.stencil.setMask(4294967295)),R.clear(r)},this.clearColor=function(){this.clear(!0,!1,!1)},this.clearDepth=function(){this.clear(!1,!0,!1)},this.clearStencil=function(){this.clear(!1,!1,!0)},this.dispose=function(){t.removeEventListener(`webglcontextlost`,ke,!1),t.removeEventListener(`webglcontextrestored`,Ae,!1),t.removeEventListener(`webglcontextcreationerror`,je,!1),_e.dispose(),ve.dispose(),U.dispose(),ue.dispose(),de.dispose(),me.dispose(),Te.dispose(),Ee.dispose(),he.dispose(),Oe.dispose(),Oe.removeEventListener(`sessionstart`,Re),Oe.removeEventListener(`sessionend`,ze),ie&&=(ie.dispose(),null),Be.stop()};function ke(e){e.preventDefault(),console.log(`THREE.WebGLRenderer: Context Lost.`),y=!0}function Ae(){console.log(`THREE.WebGLRenderer: Context Restored.`),y=!1;let e=H.autoReset,t=be.enabled,n=be.autoUpdate,r=be.needsUpdate,i=be.type;De(),H.autoReset=e,be.enabled=t,be.autoUpdate=n,be.needsUpdate=r,be.type=i}function je(e){console.error(`THREE.WebGLRenderer: A WebGL context could not be created. Reason: `,e.statusMessage)}function Me(e){let t=e.target;t.removeEventListener(`dispose`,Me),Ne(t)}function Ne(e){Pe(e),U.remove(e)}function Pe(e){let t=U.get(e).programs;t!==void 0&&(t.forEach(function(e){he.releaseProgram(e)}),e.isShaderMaterial&&he.releaseShaderCache(e))}this.renderBufferDirect=function(e,t,n,r,i,a){t===null&&(t=se);let o=i.isMesh&&i.matrixWorld.determinant()<0,s=Je(e,t,n,r,i);V.setMaterial(r,o);let c=n.index,l=1;if(r.wireframe===!0){if(c=pe.getWireframeAttribute(n),c===void 0)return;l=2}let u=n.drawRange,d=n.attributes.position,f=u.start*l,p=(u.start+u.count)*l;a!==null&&(f=Math.max(f,a.start*l),p=Math.min(p,(a.start+a.count)*l)),c===null?d!=null&&(f=Math.max(f,0),p=Math.min(p,d.count)):(f=Math.max(f,0),p=Math.min(p,c.count));let m=p-f;if(m<0||m===1/0)return;Te.setup(i,r,s,n,c);let h,g=G;if(c!==null&&(h=fe.get(c),g=Ce,g.setIndex(h)),i.isMesh)r.wireframe===!0?(V.setLineWidth(r.wireframeLinewidth*ce()),g.setMode(R.LINES)):g.setMode(R.TRIANGLES);else if(i.isLine){let e=r.linewidth;e===void 0&&(e=1),V.setLineWidth(e*ce()),i.isLineSegments?g.setMode(R.LINES):i.isLineLoop?g.setMode(R.LINE_LOOP):g.setMode(R.LINE_STRIP)}else i.isPoints?g.setMode(R.POINTS):i.isSprite&&g.setMode(R.TRIANGLES);if(i.isBatchedMesh)g.renderMultiDraw(i._multiDrawStarts,i._multiDrawCounts,i._multiDrawCount);else if(i.isInstancedMesh)g.renderInstances(f,m,i.count);else if(n.isInstancedBufferGeometry){let e=n._maxInstanceCount===void 0?1/0:n._maxInstanceCount,t=Math.min(n.instanceCount,e);g.renderInstances(f,m,t)}else g.render(f,m)};function Fe(e,t,n){e.transparent===!0&&e.side===2&&e.forceSinglePass===!1?(e.side=1,e.needsUpdate=!0,Ge(e,t,n),e.side=0,e.needsUpdate=!0,Ge(e,t,n),e.side=2):Ge(e,t,n)}this.compile=function(e,t,n=null){n===null&&(n=e),h=ve.get(n),h.init(),_.push(h),n.traverseVisible(function(e){e.isLight&&e.layers.test(t.layers)&&(h.pushLight(e),e.castShadow&&h.pushShadow(e))}),e!==n&&e.traverseVisible(function(e){e.isLight&&e.layers.test(t.layers)&&(h.pushLight(e),e.castShadow&&h.pushShadow(e))}),h.setupLights(v._useLegacyLights);let r=new Set;return e.traverse(function(e){let t=e.material;if(t)if(Array.isArray(t))for(let i=0;i<t.length;i++){let a=t[i];Fe(a,n,e),r.add(a)}else Fe(t,n,e),r.add(t)}),_.pop(),h=null,r},this.compileAsync=function(e,t,n=null){let r=this.compile(e,t,n);return new Promise(t=>{function n(){if(r.forEach(function(e){U.get(e).currentProgram.isReady()&&r.delete(e)}),r.size===0){t(e);return}setTimeout(n,10)}z.get(`KHR_parallel_shader_compile`)===null?setTimeout(n,10):n()})};let Ie=null;function Le(e){Ie&&Ie(e)}function Re(){Be.stop()}function ze(){Be.start()}let Be=new Po;Be.setAnimationLoop(Le),typeof self<`u`&&Be.setContext(self),this.setAnimationLoop=function(e){Ie=e,Oe.setAnimationLoop(e),e===null?Be.stop():Be.start()},Oe.addEventListener(`sessionstart`,Re),Oe.addEventListener(`sessionend`,ze),this.render=function(e,t){if(t!==void 0&&t.isCamera!==!0){console.error(`THREE.WebGLRenderer.render: camera is not an instance of THREE.Camera.`);return}if(y===!0)return;e.matrixWorldAutoUpdate===!0&&e.updateMatrixWorld(),t.parent===null&&t.matrixWorldAutoUpdate===!0&&t.updateMatrixWorld(),Oe.enabled===!0&&Oe.isPresenting===!0&&(Oe.cameraAutoUpdate===!0&&Oe.updateCamera(t),t=Oe.getCamera()),e.isScene===!0&&e.onBeforeRender(v,e,t,S),h=ve.get(e,_.length),h.init(),_.push(h),ae.multiplyMatrices(t.projectionMatrix,t.matrixWorldInverse),te.setFromProjectionMatrix(ae),re=this.localClippingEnabled,ne=ye.init(this.clippingPlanes,re),m=_e.get(e,g.length),m.init(),g.push(m),Ve(e,t,0,v.sortObjects),m.finish(),v.sortObjects===!0&&m.sort(N,P),this.info.render.frame++,ne===!0&&ye.beginShadows();let n=h.state.shadowsArray;if(be.render(n,e,t),ne===!0&&ye.endShadows(),this.info.autoReset===!0&&this.info.reset(),(Oe.enabled===!1||Oe.isPresenting===!1||Oe.hasDepthSensing()===!1)&&xe.render(m,e),h.setupLights(v._useLegacyLights),t.isArrayCamera){let n=t.cameras;for(let t=0,r=n.length;t<r;t++){let r=n[t];He(m,e,r,r.viewport)}}else He(m,e,t);S!==null&&(W.updateMultisampleRenderTarget(S),W.updateRenderTargetMipmap(S)),e.isScene===!0&&e.onAfterRender(v,e,t),Te.resetDefaultState(),C=-1,w=null,_.pop(),h=_.length>0?_[_.length-1]:null,g.pop(),m=g.length>0?g[g.length-1]:null};function Ve(e,t,n,r){if(e.visible===!1)return;if(e.layers.test(t.layers)){if(e.isGroup)n=e.renderOrder;else if(e.isLOD)e.autoUpdate===!0&&e.update(t);else if(e.isLight)h.pushLight(e),e.castShadow&&h.pushShadow(e);else if(e.isSprite){if(!e.frustumCulled||te.intersectsSprite(e)){r&&L.setFromMatrixPosition(e.matrixWorld).applyMatrix4(ae);let t=me.update(e),i=e.material;i.visible&&m.push(e,t,i,n,L.z,null)}}else if((e.isMesh||e.isLine||e.isPoints)&&(!e.frustumCulled||te.intersectsObject(e))){let t=me.update(e),i=e.material;if(r&&(e.boundingSphere===void 0?(t.boundingSphere===null&&t.computeBoundingSphere(),L.copy(t.boundingSphere.center)):(e.boundingSphere===null&&e.computeBoundingSphere(),L.copy(e.boundingSphere.center)),L.applyMatrix4(e.matrixWorld).applyMatrix4(ae)),Array.isArray(i)){let r=t.groups;for(let a=0,o=r.length;a<o;a++){let o=r[a],s=i[o.materialIndex];s&&s.visible&&m.push(e,t,s,n,L.z,o)}}else i.visible&&m.push(e,t,i,n,L.z,null)}}let i=e.children;for(let e=0,a=i.length;e<a;e++)Ve(i[e],t,n,r)}function He(e,t,n,r){let i=e.opaque,a=e.transmissive,o=e.transparent;h.setupLightsView(n),ne===!0&&ye.setGlobalState(v.clippingPlanes,n),a.length>0&&Ue(i,a,t,n),r&&V.viewport(T.copy(r)),i.length>0&&K(i,t,n),a.length>0&&K(a,t,n),o.length>0&&K(o,t,n),V.buffers.depth.setTest(!0),V.buffers.depth.setMask(!0),V.buffers.color.setMask(!0),V.setPolygonOffset(!1)}function Ue(e,t,n,r){if((n.isScene===!0?n.overrideMaterial:null)!==null)return;let i=B.isWebGL2;ie===null&&(ie=new ei(1,1,{generateMipmaps:!0,type:z.has(`EXT_color_buffer_half_float`)?Pn:jn,minFilter:An,samples:i?4:0})),v.getDrawingBufferSize(oe),i?ie.setSize(oe.x,oe.y):ie.setSize(Dr(oe.x),Dr(oe.y));let a=v.getRenderTarget();v.setRenderTarget(ie),v.getClearColor(O),k=v.getClearAlpha(),k<1&&v.setClearColor(16777215,.5),v.clear();let o=v.toneMapping;v.toneMapping=0,K(e,n,r),W.updateMultisampleRenderTarget(ie),W.updateRenderTargetMipmap(ie);let s=!1;for(let e=0,i=t.length;e<i;e++){let i=t[e],a=i.object,o=i.geometry,c=i.material,l=i.group;if(c.side===2&&a.layers.test(r.layers)){let e=c.side;c.side=1,c.needsUpdate=!0,We(a,n,r,o,c,l),c.side=e,c.needsUpdate=!0,s=!0}}s===!0&&(W.updateMultisampleRenderTarget(ie),W.updateRenderTargetMipmap(ie)),v.setRenderTarget(a),v.setClearColor(O,k),v.toneMapping=o}function K(e,t,n){let r=t.isScene===!0?t.overrideMaterial:null;for(let i=0,a=e.length;i<a;i++){let a=e[i],o=a.object,s=a.geometry,c=r===null?a.material:r,l=a.group;o.layers.test(n.layers)&&We(o,t,n,s,c,l)}}function We(e,t,n,r,i,a){e.onBeforeRender(v,t,n,r,i,a),e.modelViewMatrix.multiplyMatrices(n.matrixWorldInverse,e.matrixWorld),e.normalMatrix.getNormalMatrix(e.modelViewMatrix),i.onBeforeRender(v,t,n,r,e,a),i.transparent===!0&&i.side===2&&i.forceSinglePass===!1?(i.side=1,i.needsUpdate=!0,v.renderBufferDirect(n,t,r,i,e,a),i.side=0,i.needsUpdate=!0,v.renderBufferDirect(n,t,r,i,e,a),i.side=2):v.renderBufferDirect(n,t,r,i,e,a),e.onAfterRender(v,t,n,r,i,a)}function Ge(e,t,n){t.isScene!==!0&&(t=se);let r=U.get(e),i=h.state.lights,a=h.state.shadowsArray,o=i.state.version,s=he.getParameters(e,i.state,a,t,n),c=he.getProgramCacheKey(s),l=r.programs;r.environment=e.isMeshStandardMaterial?t.environment:null,r.fog=t.fog,r.envMap=(e.isMeshStandardMaterial?de:ue).get(e.envMap||r.environment),r.envMapRotation=r.environment!==null&&e.envMap===null?t.environmentRotation:e.envMapRotation,l===void 0&&(e.addEventListener(`dispose`,Me),l=new Map,r.programs=l);let u=l.get(c);if(u!==void 0){if(r.currentProgram===u&&r.lightsStateVersion===o)return qe(e,s),u}else s.uniforms=he.getUniforms(e),e.onBuild(n,s,v),e.onBeforeCompile(s,v),u=he.acquireProgram(s,c),l.set(c,u),r.uniforms=s.uniforms;let d=r.uniforms;return(!e.isShaderMaterial&&!e.isRawShaderMaterial||e.clipping===!0)&&(d.clippingPlanes=ye.uniform),qe(e,s),r.needsLights=Ye(e),r.lightsStateVersion=o,r.needsLights&&(d.ambientLightColor.value=i.state.ambient,d.lightProbe.value=i.state.probe,d.directionalLights.value=i.state.directional,d.directionalLightShadows.value=i.state.directionalShadow,d.spotLights.value=i.state.spot,d.spotLightShadows.value=i.state.spotShadow,d.rectAreaLights.value=i.state.rectArea,d.ltc_1.value=i.state.rectAreaLTC1,d.ltc_2.value=i.state.rectAreaLTC2,d.pointLights.value=i.state.point,d.pointLightShadows.value=i.state.pointShadow,d.hemisphereLights.value=i.state.hemi,d.directionalShadowMap.value=i.state.directionalShadowMap,d.directionalShadowMatrix.value=i.state.directionalShadowMatrix,d.spotShadowMap.value=i.state.spotShadowMap,d.spotLightMatrix.value=i.state.spotLightMatrix,d.spotLightMap.value=i.state.spotLightMap,d.pointShadowMap.value=i.state.pointShadowMap,d.pointShadowMatrix.value=i.state.pointShadowMatrix),r.currentProgram=u,r.uniformsList=null,u}function Ke(e){if(e.uniformsList===null){let t=e.currentProgram.getUniforms();e.uniformsList=kc.seqWithValue(t.seq,e.uniforms)}return e.uniformsList}function qe(e,t){let n=U.get(e);n.outputColorSpace=t.outputColorSpace,n.batching=t.batching,n.instancing=t.instancing,n.instancingColor=t.instancingColor,n.instancingMorph=t.instancingMorph,n.skinning=t.skinning,n.morphTargets=t.morphTargets,n.morphNormals=t.morphNormals,n.morphColors=t.morphColors,n.morphTargetsCount=t.morphTargetsCount,n.numClippingPlanes=t.numClippingPlanes,n.numIntersection=t.numClipIntersection,n.vertexAlphas=t.vertexAlphas,n.vertexTangents=t.vertexTangents,n.toneMapping=t.toneMapping}function Je(e,t,n,r,i){t.isScene!==!0&&(t=se),W.resetTextureUnits();let a=t.fog,o=r.isMeshStandardMaterial?t.environment:null,s=S===null?v.outputColorSpace:S.isXRRenderTarget===!0?S.texture.colorSpace:Jn,c=(r.isMeshStandardMaterial?de:ue).get(r.envMap||o),l=r.vertexColors===!0&&!!n.attributes.color&&n.attributes.color.itemSize===4,u=!!n.attributes.tangent&&(!!r.normalMap||r.anisotropy>0),d=!!n.morphAttributes.position,f=!!n.morphAttributes.normal,p=!!n.morphAttributes.color,m=0;r.toneMapped&&(S===null||S.isXRRenderTarget===!0)&&(m=v.toneMapping);let g=n.morphAttributes.position||n.morphAttributes.normal||n.morphAttributes.color,_=g===void 0?0:g.length,y=U.get(r),b=h.state.lights;if(ne===!0&&(re===!0||e!==w)){let t=e===w&&r.id===C;ye.setState(r,e,t)}let x=!1;r.version===y.__version?y.needsLights&&y.lightsStateVersion!==b.state.version?x=!0:y.outputColorSpace===s?i.isBatchedMesh&&y.batching===!1||!i.isBatchedMesh&&y.batching===!0||i.isInstancedMesh&&y.instancing===!1||!i.isInstancedMesh&&y.instancing===!0||i.isSkinnedMesh&&y.skinning===!1||!i.isSkinnedMesh&&y.skinning===!0||i.isInstancedMesh&&y.instancingColor===!0&&i.instanceColor===null||i.isInstancedMesh&&y.instancingColor===!1&&i.instanceColor!==null||i.isInstancedMesh&&y.instancingMorph===!0&&i.morphTexture===null||i.isInstancedMesh&&y.instancingMorph===!1&&i.morphTexture!==null?x=!0:y.envMap===c?r.fog===!0&&y.fog!==a||y.numClippingPlanes!==void 0&&(y.numClippingPlanes!==ye.numPlanes||y.numIntersection!==ye.numIntersection)?x=!0:y.vertexAlphas===l&&y.vertexTangents===u&&y.morphTargets===d&&y.morphNormals===f&&y.morphColors===p&&y.toneMapping===m?B.isWebGL2===!0&&y.morphTargetsCount!==_&&(x=!0):x=!0:x=!0:x=!0:(x=!0,y.__version=r.version);let T=y.currentProgram;x===!0&&(T=Ge(r,t,i));let E=!1,D=!1,O=!1,k=T.getUniforms(),A=y.uniforms;if(V.useProgram(T.program)&&(E=!0,D=!0,O=!0),r.id!==C&&(C=r.id,D=!0),E||w!==e){k.setValue(R,`projectionMatrix`,e.projectionMatrix),k.setValue(R,`viewMatrix`,e.matrixWorldInverse);let t=k.map.cameraPosition;t!==void 0&&t.setValue(R,L.setFromMatrixPosition(e.matrixWorld)),B.logarithmicDepthBuffer&&k.setValue(R,`logDepthBufFC`,2/(Math.log(e.far+1)/Math.LN2)),(r.isMeshPhongMaterial||r.isMeshToonMaterial||r.isMeshLambertMaterial||r.isMeshBasicMaterial||r.isMeshStandardMaterial||r.isShaderMaterial)&&k.setValue(R,`isOrthographic`,e.isOrthographicCamera===!0),w!==e&&(w=e,D=!0,O=!0)}if(i.isSkinnedMesh){k.setOptional(R,i,`bindMatrix`),k.setOptional(R,i,`bindMatrixInverse`);let e=i.skeleton;e&&(B.floatVertexTextures?(e.boneTexture===null&&e.computeBoneTexture(),k.setValue(R,`boneTexture`,e.boneTexture,W)):console.warn(`THREE.WebGLRenderer: SkinnedMesh can only be used with WebGL 2. With WebGL 1 OES_texture_float and vertex textures support is required.`))}i.isBatchedMesh&&(k.setOptional(R,i,`batchingTexture`),k.setValue(R,`batchingTexture`,i._matricesTexture,W));let N=n.morphAttributes;if((N.position!==void 0||N.normal!==void 0||N.color!==void 0&&B.isWebGL2===!0)&&Se.update(i,n,T),(D||y.receiveShadow!==i.receiveShadow)&&(y.receiveShadow=i.receiveShadow,k.setValue(R,`receiveShadow`,i.receiveShadow)),r.isMeshGouraudMaterial&&r.envMap!==null&&(A.envMap.value=c,A.flipEnvMap.value=c.isCubeTexture&&c.isRenderTargetTexture===!1?-1:1),D&&(k.setValue(R,`toneMappingExposure`,v.toneMappingExposure),y.needsLights&&q(A,O),a&&r.fog===!0&&ge.refreshFogUniforms(A,a),ge.refreshMaterialUniforms(A,r,M,j,ie),kc.upload(R,Ke(y),A,W)),r.isShaderMaterial&&r.uniformsNeedUpdate===!0&&(kc.upload(R,Ke(y),A,W),r.uniformsNeedUpdate=!1),r.isSpriteMaterial&&k.setValue(R,`center`,i.center),k.setValue(R,`modelViewMatrix`,i.modelViewMatrix),k.setValue(R,`normalMatrix`,i.normalMatrix),k.setValue(R,`modelMatrix`,i.matrixWorld),r.isShaderMaterial||r.isRawShaderMaterial){let e=r.uniformsGroups;for(let t=0,n=e.length;t<n;t++)if(B.isWebGL2){let n=e[t];Ee.update(n,T),Ee.bind(n,T)}else console.warn(`THREE.WebGLRenderer: Uniform Buffer Objects can only be used with WebGL 2.`)}return T}function q(e,t){e.ambientLightColor.needsUpdate=t,e.lightProbe.needsUpdate=t,e.directionalLights.needsUpdate=t,e.directionalLightShadows.needsUpdate=t,e.pointLights.needsUpdate=t,e.pointLightShadows.needsUpdate=t,e.spotLights.needsUpdate=t,e.spotLightShadows.needsUpdate=t,e.rectAreaLights.needsUpdate=t,e.hemisphereLights.needsUpdate=t}function Ye(e){return e.isMeshLambertMaterial||e.isMeshToonMaterial||e.isMeshPhongMaterial||e.isMeshStandardMaterial||e.isShadowMaterial||e.isShaderMaterial&&e.lights===!0}this.getActiveCubeFace=function(){return b},this.getActiveMipmapLevel=function(){return x},this.getRenderTarget=function(){return S},this.setRenderTargetTextures=function(e,t,n){U.get(e.texture).__webglTexture=t,U.get(e.depthTexture).__webglTexture=n;let r=U.get(e);r.__hasExternalTextures=!0,r.__autoAllocateDepthBuffer=n===void 0,r.__autoAllocateDepthBuffer||z.has(`WEBGL_multisampled_render_to_texture`)===!0&&(console.warn(`THREE.WebGLRenderer: Render-to-texture extension was disabled because an external texture was provided`),r.__useRenderToTexture=!1)},this.setRenderTargetFramebuffer=function(e,t){let n=U.get(e);n.__webglFramebuffer=t,n.__useDefaultFramebuffer=t===void 0},this.setRenderTarget=function(e,t=0,n=0){S=e,b=t,x=n;let r=!0,i=null,a=!1,o=!1;if(e){let s=U.get(e);s.__useDefaultFramebuffer===void 0?s.__webglFramebuffer===void 0?W.setupRenderTarget(e):s.__hasExternalTextures&&W.rebindTextures(e,U.get(e.texture).__webglTexture,U.get(e.depthTexture).__webglTexture):(V.bindFramebuffer(R.FRAMEBUFFER,null),r=!1);let c=e.texture;(c.isData3DTexture||c.isDataArrayTexture||c.isCompressedArrayTexture)&&(o=!0);let l=U.get(e).__webglFramebuffer;e.isWebGLCubeRenderTarget?(i=Array.isArray(l[t])?l[t][n]:l[t],a=!0):i=B.isWebGL2&&e.samples>0&&W.useMultisampledRTT(e)===!1?U.get(e).__webglMultisampledFramebuffer:Array.isArray(l)?l[n]:l,T.copy(e.viewport),E.copy(e.scissor),D=e.scissorTest}else T.copy(F).multiplyScalar(M).floor(),E.copy(ee).multiplyScalar(M).floor(),D=I;if(V.bindFramebuffer(R.FRAMEBUFFER,i)&&B.drawBuffers&&r&&V.drawBuffers(e,i),V.viewport(T),V.scissor(E),V.setScissorTest(D),a){let r=U.get(e.texture);R.framebufferTexture2D(R.FRAMEBUFFER,R.COLOR_ATTACHMENT0,R.TEXTURE_CUBE_MAP_POSITIVE_X+t,r.__webglTexture,n)}else if(o){let r=U.get(e.texture),i=t||0;R.framebufferTextureLayer(R.FRAMEBUFFER,R.COLOR_ATTACHMENT0,r.__webglTexture,n||0,i)}C=-1},this.readRenderTargetPixels=function(e,t,n,r,i,a,o){if(!(e&&e.isWebGLRenderTarget)){console.error(`THREE.WebGLRenderer.readRenderTargetPixels: renderTarget is not THREE.WebGLRenderTarget.`);return}let s=U.get(e).__webglFramebuffer;if(e.isWebGLCubeRenderTarget&&o!==void 0&&(s=s[o]),s){V.bindFramebuffer(R.FRAMEBUFFER,s);try{let o=e.texture,s=o.format,c=o.type;if(s!==1023&&we.convert(s)!==R.getParameter(R.IMPLEMENTATION_COLOR_READ_FORMAT)){console.error(`THREE.WebGLRenderer.readRenderTargetPixels: renderTarget is not in RGBA or implementation defined format.`);return}let l=c===1016&&(z.has(`EXT_color_buffer_half_float`)||B.isWebGL2&&z.has(`EXT_color_buffer_float`));if(c!==1009&&we.convert(c)!==R.getParameter(R.IMPLEMENTATION_COLOR_READ_TYPE)&&!(c===1015&&(B.isWebGL2||z.has(`OES_texture_float`)||z.has(`WEBGL_color_buffer_float`)))&&!l){console.error(`THREE.WebGLRenderer.readRenderTargetPixels: renderTarget is not in UnsignedByteType or implementation defined type.`);return}t>=0&&t<=e.width-r&&n>=0&&n<=e.height-i&&R.readPixels(t,n,r,i,we.convert(s),we.convert(c),a)}finally{let e=S===null?null:U.get(S).__webglFramebuffer;V.bindFramebuffer(R.FRAMEBUFFER,e)}}},this.copyFramebufferToTexture=function(e,t,n=0){let r=2**-n,i=Math.floor(t.image.width*r),a=Math.floor(t.image.height*r);W.setTexture2D(t,0),R.copyTexSubImage2D(R.TEXTURE_2D,n,0,0,e.x,e.y,i,a),V.unbindTexture()},this.copyTextureToTexture=function(e,t,n,r=0){let i=t.image.width,a=t.image.height,o=we.convert(n.format),s=we.convert(n.type);W.setTexture2D(n,0),R.pixelStorei(R.UNPACK_FLIP_Y_WEBGL,n.flipY),R.pixelStorei(R.UNPACK_PREMULTIPLY_ALPHA_WEBGL,n.premultiplyAlpha),R.pixelStorei(R.UNPACK_ALIGNMENT,n.unpackAlignment),t.isDataTexture?R.texSubImage2D(R.TEXTURE_2D,r,e.x,e.y,i,a,o,s,t.image.data):t.isCompressedTexture?R.compressedTexSubImage2D(R.TEXTURE_2D,r,e.x,e.y,t.mipmaps[0].width,t.mipmaps[0].height,o,t.mipmaps[0].data):R.texSubImage2D(R.TEXTURE_2D,r,e.x,e.y,o,s,t.image),r===0&&n.generateMipmaps&&R.generateMipmap(R.TEXTURE_2D),V.unbindTexture()},this.copyTextureToTexture3D=function(e,t,n,r,i=0){if(v.isWebGL1Renderer){console.warn(`THREE.WebGLRenderer.copyTextureToTexture3D: can only be used with WebGL2.`);return}let a=Math.round(e.max.x-e.min.x),o=Math.round(e.max.y-e.min.y),s=e.max.z-e.min.z+1,c=we.convert(r.format),l=we.convert(r.type),u;if(r.isData3DTexture)W.setTexture3D(r,0),u=R.TEXTURE_3D;else if(r.isDataArrayTexture||r.isCompressedArrayTexture)W.setTexture2DArray(r,0),u=R.TEXTURE_2D_ARRAY;else{console.warn(`THREE.WebGLRenderer.copyTextureToTexture3D: only supports THREE.DataTexture3D and THREE.DataTexture2DArray.`);return}R.pixelStorei(R.UNPACK_FLIP_Y_WEBGL,r.flipY),R.pixelStorei(R.UNPACK_PREMULTIPLY_ALPHA_WEBGL,r.premultiplyAlpha),R.pixelStorei(R.UNPACK_ALIGNMENT,r.unpackAlignment);let d=R.getParameter(R.UNPACK_ROW_LENGTH),f=R.getParameter(R.UNPACK_IMAGE_HEIGHT),p=R.getParameter(R.UNPACK_SKIP_PIXELS),m=R.getParameter(R.UNPACK_SKIP_ROWS),h=R.getParameter(R.UNPACK_SKIP_IMAGES),g=n.isCompressedTexture?n.mipmaps[i]:n.image;R.pixelStorei(R.UNPACK_ROW_LENGTH,g.width),R.pixelStorei(R.UNPACK_IMAGE_HEIGHT,g.height),R.pixelStorei(R.UNPACK_SKIP_PIXELS,e.min.x),R.pixelStorei(R.UNPACK_SKIP_ROWS,e.min.y),R.pixelStorei(R.UNPACK_SKIP_IMAGES,e.min.z),n.isDataTexture||n.isData3DTexture?R.texSubImage3D(u,i,t.x,t.y,t.z,a,o,s,c,l,g.data):r.isCompressedArrayTexture?R.compressedTexSubImage3D(u,i,t.x,t.y,t.z,a,o,s,c,g.data):R.texSubImage3D(u,i,t.x,t.y,t.z,a,o,s,c,l,g),R.pixelStorei(R.UNPACK_ROW_LENGTH,d),R.pixelStorei(R.UNPACK_IMAGE_HEIGHT,f),R.pixelStorei(R.UNPACK_SKIP_PIXELS,p),R.pixelStorei(R.UNPACK_SKIP_ROWS,m),R.pixelStorei(R.UNPACK_SKIP_IMAGES,h),i===0&&r.generateMipmaps&&R.generateMipmap(u),V.unbindTexture()},this.initTexture=function(e){e.isCubeTexture?W.setTextureCube(e,0):e.isData3DTexture?W.setTexture3D(e,0):e.isDataArrayTexture||e.isCompressedArrayTexture?W.setTexture2DArray(e,0):W.setTexture2D(e,0),V.unbindTexture()},this.resetState=function(){b=0,x=0,S=null,V.reset(),Te.reset()},typeof __THREE_DEVTOOLS__<`u`&&__THREE_DEVTOOLS__.dispatchEvent(new CustomEvent(`observe`,{detail:this}))}get coordinateSystem(){return rr}get outputColorSpace(){return this._outputColorSpace}set outputColorSpace(e){this._outputColorSpace=e;let t=this.getContext();t.drawingBufferColorSpace=e===`display-p3`?`display-p3`:`srgb`,t.unpackColorSpace=Hr.workingColorSpace===`display-p3-linear`?`display-p3`:`srgb`}get useLegacyLights(){return console.warn(`THREE.WebGLRenderer: The property .useLegacyLights has been deprecated. Migrate your lighting according to the following guide: https://discourse.threejs.org/t/updates-to-lighting-in-three-js-r155/53733.`),this._useLegacyLights}set useLegacyLights(e){console.warn(`THREE.WebGLRenderer: The property .useLegacyLights has been deprecated. Migrate your lighting according to the following guide: https://discourse.threejs.org/t/updates-to-lighting-in-three-js-r155/53733.`),this._useLegacyLights=e}},Hl=class extends Vl{};Hl.prototype.isWebGL1Renderer=!0;var Ul=class extends oa{constructor(){super(),this.isScene=!0,this.type=`Scene`,this.background=null,this.environment=null,this.fog=null,this.backgroundBlurriness=0,this.backgroundIntensity=1,this.backgroundRotation=new Ui,this.environmentRotation=new Ui,this.overrideMaterial=null,typeof __THREE_DEVTOOLS__<`u`&&__THREE_DEVTOOLS__.dispatchEvent(new CustomEvent(`observe`,{detail:this}))}copy(e,t){return super.copy(e,t),e.background!==null&&(this.background=e.background.clone()),e.environment!==null&&(this.environment=e.environment.clone()),e.fog!==null&&(this.fog=e.fog.clone()),this.backgroundBlurriness=e.backgroundBlurriness,this.backgroundIntensity=e.backgroundIntensity,this.backgroundRotation.copy(e.backgroundRotation),this.environmentRotation.copy(e.environmentRotation),e.overrideMaterial!==null&&(this.overrideMaterial=e.overrideMaterial.clone()),this.matrixAutoUpdate=e.matrixAutoUpdate,this}toJSON(e){let t=super.toJSON(e);return this.fog!==null&&(t.object.fog=this.fog.toJSON()),this.backgroundBlurriness>0&&(t.object.backgroundBlurriness=this.backgroundBlurriness),this.backgroundIntensity!==1&&(t.object.backgroundIntensity=this.backgroundIntensity),t.object.backgroundRotation=this.backgroundRotation.toArray(),t.object.environmentRotation=this.environmentRotation.toArray(),t}},Wl=class extends Ta{constructor(e){super(),this.isMeshNormalMaterial=!0,this.type=`MeshNormalMaterial`,this.bumpMap=null,this.bumpScale=1,this.normalMap=null,this.normalMapType=0,this.normalScale=new Y(1,1),this.displacementMap=null,this.displacementScale=1,this.displacementBias=0,this.wireframe=!1,this.wireframeLinewidth=1,this.flatShading=!1,this.setValues(e)}copy(e){return super.copy(e),this.bumpMap=e.bumpMap,this.bumpScale=e.bumpScale,this.normalMap=e.normalMap,this.normalMapType=e.normalMapType,this.normalScale.copy(e.normalScale),this.displacementMap=e.displacementMap,this.displacementScale=e.displacementScale,this.displacementBias=e.displacementBias,this.wireframe=e.wireframe,this.wireframeLinewidth=e.wireframeLinewidth,this.flatShading=e.flatShading,this}};function Gl(e,t,n){return!e||!n&&e.constructor===t?e:typeof t.BYTES_PER_ELEMENT==`number`?new t(e):Array.prototype.slice.call(e)}function Kl(e){return ArrayBuffer.isView(e)&&!(e instanceof DataView)}var ql=class{constructor(e,t,n,r){this.parameterPositions=e,this._cachedIndex=0,this.resultBuffer=r===void 0?new t.constructor(n):r,this.sampleValues=t,this.valueSize=n,this.settings=null,this.DefaultSettings_={}}evaluate(e){let t=this.parameterPositions,n=this._cachedIndex,r=t[n],i=t[n-1];validate_interval:{seek:{let a;linear_scan:{forward_scan:if(!(e<r)){for(let a=n+2;;){if(r===void 0){if(e<i)break forward_scan;return n=t.length,this._cachedIndex=n,this.copySampleValue_(n-1)}if(n===a)break;if(i=r,r=t[++n],e<r)break seek}a=t.length;break linear_scan}if(!(e>=i)){let o=t[1];e<o&&(n=2,i=o);for(let a=n-2;;){if(i===void 0)return this._cachedIndex=0,this.copySampleValue_(0);if(n===a)break;if(r=i,i=t[--n-1],e>=i)break seek}a=n,n=0;break linear_scan}break validate_interval}for(;n<a;){let r=n+a>>>1;e<t[r]?a=r:n=r+1}if(r=t[n],i=t[n-1],i===void 0)return this._cachedIndex=0,this.copySampleValue_(0);if(r===void 0)return n=t.length,this._cachedIndex=n,this.copySampleValue_(n-1)}this._cachedIndex=n,this.intervalChanged_(n,i,r)}return this.interpolate_(n,i,e,r)}getSettings_(){return this.settings||this.DefaultSettings_}copySampleValue_(e){let t=this.resultBuffer,n=this.sampleValues,r=this.valueSize,i=e*r;for(let e=0;e!==r;++e)t[e]=n[i+e];return t}interpolate_(){throw Error(`call to abstract method`)}intervalChanged_(){}},Jl=class extends ql{constructor(e,t,n,r){super(e,t,n,r),this._weightPrev=-0,this._offsetPrev=-0,this._weightNext=-0,this._offsetNext=-0,this.DefaultSettings_={endingStart:Hn,endingEnd:Hn}}intervalChanged_(e,t,n){let r=this.parameterPositions,i=e-2,a=e+1,o=r[i],s=r[a];if(o===void 0)switch(this.getSettings_().endingStart){case Un:i=e,o=2*t-n;break;case Wn:i=r.length-2,o=t+r[i]-r[i+1];break;default:i=e,o=n}if(s===void 0)switch(this.getSettings_().endingEnd){case Un:a=e,s=2*n-t;break;case Wn:a=1,s=n+r[1]-r[0];break;default:a=e-1,s=t}let c=(n-t)*.5,l=this.valueSize;this._weightPrev=c/(t-o),this._weightNext=c/(s-n),this._offsetPrev=i*l,this._offsetNext=a*l}interpolate_(e,t,n,r){let i=this.resultBuffer,a=this.sampleValues,o=this.valueSize,s=e*o,c=s-o,l=this._offsetPrev,u=this._offsetNext,d=this._weightPrev,f=this._weightNext,p=(n-t)/(r-t),m=p*p,h=m*p,g=-d*h+2*d*m-d*p,_=(1+d)*h+(-1.5-2*d)*m+(-.5+d)*p+1,v=(-1-f)*h+(1.5+f)*m+.5*p,y=f*h-f*m;for(let e=0;e!==o;++e)i[e]=g*a[l+e]+_*a[c+e]+v*a[s+e]+y*a[u+e];return i}},Yl=class extends ql{constructor(e,t,n,r){super(e,t,n,r)}interpolate_(e,t,n,r){let i=this.resultBuffer,a=this.sampleValues,o=this.valueSize,s=e*o,c=s-o,l=(n-t)/(r-t),u=1-l;for(let e=0;e!==o;++e)i[e]=a[c+e]*u+a[s+e]*l;return i}},Xl=class extends ql{constructor(e,t,n,r){super(e,t,n,r)}interpolate_(e){return this.copySampleValue_(e-1)}},Zl=class{constructor(e,t,n,r){if(e===void 0)throw Error(`THREE.KeyframeTrack: track name is undefined`);if(t===void 0||t.length===0)throw Error(`THREE.KeyframeTrack: no keyframes in track named `+e);this.name=e,this.times=Gl(t,this.TimeBufferType),this.values=Gl(n,this.ValueBufferType),this.setInterpolation(r||this.DefaultInterpolation)}static toJSON(e){let t=e.constructor,n;if(t.toJSON!==this.toJSON)n=t.toJSON(e);else{n={name:e.name,times:Gl(e.times,Array),values:Gl(e.values,Array)};let t=e.getInterpolation();t!==e.DefaultInterpolation&&(n.interpolation=t)}return n.type=e.ValueTypeName,n}InterpolantFactoryMethodDiscrete(e){return new Xl(this.times,this.values,this.getValueSize(),e)}InterpolantFactoryMethodLinear(e){return new Yl(this.times,this.values,this.getValueSize(),e)}InterpolantFactoryMethodSmooth(e){return new Jl(this.times,this.values,this.getValueSize(),e)}setInterpolation(e){let t;switch(e){case zn:t=this.InterpolantFactoryMethodDiscrete;break;case Bn:t=this.InterpolantFactoryMethodLinear;break;case Vn:t=this.InterpolantFactoryMethodSmooth;break}if(t===void 0){let t=`unsupported interpolation for `+this.ValueTypeName+` keyframe track named `+this.name;if(this.createInterpolant===void 0)if(e!==this.DefaultInterpolation)this.setInterpolation(this.DefaultInterpolation);else throw Error(t);return console.warn(`THREE.KeyframeTrack:`,t),this}return this.createInterpolant=t,this}getInterpolation(){switch(this.createInterpolant){case this.InterpolantFactoryMethodDiscrete:return zn;case this.InterpolantFactoryMethodLinear:return Bn;case this.InterpolantFactoryMethodSmooth:return Vn}}getValueSize(){return this.values.length/this.times.length}shift(e){if(e!==0){let t=this.times;for(let n=0,r=t.length;n!==r;++n)t[n]+=e}return this}scale(e){if(e!==1){let t=this.times;for(let n=0,r=t.length;n!==r;++n)t[n]*=e}return this}trim(e,t){let n=this.times,r=n.length,i=0,a=r-1;for(;i!==r&&n[i]<e;)++i;for(;a!==-1&&n[a]>t;)--a;if(++a,i!==0||a!==r){i>=a&&(a=Math.max(a,1),i=a-1);let e=this.getValueSize();this.times=n.slice(i,a),this.values=this.values.slice(i*e,a*e)}return this}validate(){let e=!0,t=this.getValueSize();t-Math.floor(t)!==0&&(console.error(`THREE.KeyframeTrack: Invalid value size in track.`,this),e=!1);let n=this.times,r=this.values,i=n.length;i===0&&(console.error(`THREE.KeyframeTrack: Track is empty.`,this),e=!1);let a=null;for(let t=0;t!==i;t++){let r=n[t];if(typeof r==`number`&&isNaN(r)){console.error(`THREE.KeyframeTrack: Time is not a valid number.`,this,t,r),e=!1;break}if(a!==null&&a>r){console.error(`THREE.KeyframeTrack: Out of order keys.`,this,t,r,a),e=!1;break}a=r}if(r!==void 0&&Kl(r))for(let t=0,n=r.length;t!==n;++t){let n=r[t];if(isNaN(n)){console.error(`THREE.KeyframeTrack: Value is not a valid number.`,this,t,n),e=!1;break}}return e}optimize(){let e=this.times.slice(),t=this.values.slice(),n=this.getValueSize(),r=this.getInterpolation()===Vn,i=e.length-1,a=1;for(let o=1;o<i;++o){let i=!1,s=e[o];if(s!==e[o+1]&&(o!==1||s!==e[0]))if(r)i=!0;else{let e=o*n,r=e-n,a=e+n;for(let o=0;o!==n;++o){let n=t[e+o];if(n!==t[r+o]||n!==t[a+o]){i=!0;break}}}if(i){if(o!==a){e[a]=e[o];let r=o*n,i=a*n;for(let e=0;e!==n;++e)t[i+e]=t[r+e]}++a}}if(i>0){e[a]=e[i];for(let e=i*n,r=a*n,o=0;o!==n;++o)t[r+o]=t[e+o];++a}return a===e.length?(this.times=e,this.values=t):(this.times=e.slice(0,a),this.values=t.slice(0,a*n)),this}clone(){let e=this.times.slice(),t=this.values.slice(),n=this.constructor,r=new n(this.name,e,t);return r.createInterpolant=this.createInterpolant,r}};Zl.prototype.TimeBufferType=Float32Array,Zl.prototype.ValueBufferType=Float32Array,Zl.prototype.DefaultInterpolation=Bn;var Ql=class extends Zl{};Ql.prototype.ValueTypeName=`bool`,Ql.prototype.ValueBufferType=Array,Ql.prototype.DefaultInterpolation=zn,Ql.prototype.InterpolantFactoryMethodLinear=void 0,Ql.prototype.InterpolantFactoryMethodSmooth=void 0;var $l=class extends Zl{};$l.prototype.ValueTypeName=`color`;var eu=class extends Zl{};eu.prototype.ValueTypeName=`number`;var tu=class extends ql{constructor(e,t,n,r){super(e,t,n,r)}interpolate_(e,t,n,r){let i=this.resultBuffer,a=this.sampleValues,o=this.valueSize,s=(n-t)/(r-t),c=e*o;for(let e=c+o;c!==e;c+=4)ri.slerpFlat(i,0,a,c-o,a,c,s);return i}},nu=class extends Zl{InterpolantFactoryMethodLinear(e){return new tu(this.times,this.values,this.getValueSize(),e)}};nu.prototype.ValueTypeName=`quaternion`,nu.prototype.DefaultInterpolation=Bn,nu.prototype.InterpolantFactoryMethodSmooth=void 0;var ru=class extends Zl{};ru.prototype.ValueTypeName=`string`,ru.prototype.ValueBufferType=Array,ru.prototype.DefaultInterpolation=zn,ru.prototype.InterpolantFactoryMethodLinear=void 0,ru.prototype.InterpolantFactoryMethodSmooth=void 0;var iu=class extends Zl{};iu.prototype.ValueTypeName=`vector`;var au=new class{constructor(e,t,n){let r=this,i=!1,a=0,o=0,s,c=[];this.onStart=void 0,this.onLoad=e,this.onProgress=t,this.onError=n,this.itemStart=function(e){o++,i===!1&&r.onStart!==void 0&&r.onStart(e,a,o),i=!0},this.itemEnd=function(e){a++,r.onProgress!==void 0&&r.onProgress(e,a,o),a===o&&(i=!1,r.onLoad!==void 0&&r.onLoad())},this.itemError=function(e){r.onError!==void 0&&r.onError(e)},this.resolveURL=function(e){return s?s(e):e},this.setURLModifier=function(e){return s=e,this},this.addHandler=function(e,t){return c.push(e,t),this},this.removeHandler=function(e){let t=c.indexOf(e);return t!==-1&&c.splice(t,2),this},this.getHandler=function(e){for(let t=0,n=c.length;t<n;t+=2){let n=c[t],r=c[t+1];if(n.global&&(n.lastIndex=0),n.test(e))return r}return null}}},ou=class{constructor(e){this.manager=e===void 0?au:e,this.crossOrigin=`anonymous`,this.withCredentials=!1,this.path=``,this.resourcePath=``,this.requestHeader={}}load(){}loadAsync(e,t){let n=this;return new Promise(function(r,i){n.load(e,r,t,i)})}parse(){}setCrossOrigin(e){return this.crossOrigin=e,this}setWithCredentials(e){return this.withCredentials=e,this}setPath(e){return this.path=e,this}setResourcePath(e){return this.resourcePath=e,this}setRequestHeader(e){return this.requestHeader=e,this}};ou.DEFAULT_MATERIAL_NAME=`__DEFAULT`;var su=class extends oa{constructor(e,t=1){super(),this.isLight=!0,this.type=`Light`,this.color=new Sa(e),this.intensity=t}dispose(){}copy(e,t){return super.copy(e,t),this.color.copy(e.color),this.intensity=e.intensity,this}toJSON(e){let t=super.toJSON(e);return t.object.color=this.color.getHex(),t.object.intensity=this.intensity,this.groundColor!==void 0&&(t.object.groundColor=this.groundColor.getHex()),this.distance!==void 0&&(t.object.distance=this.distance),this.angle!==void 0&&(t.object.angle=this.angle),this.decay!==void 0&&(t.object.decay=this.decay),this.penumbra!==void 0&&(t.object.penumbra=this.penumbra),this.shadow!==void 0&&(t.object.shadow=this.shadow.toJSON()),t}},cu=new Ni,lu=new Z,uu=new Z,du=class{constructor(e){this.camera=e,this.bias=0,this.normalBias=0,this.radius=1,this.blurSamples=8,this.mapSize=new Y(512,512),this.map=null,this.mapPass=null,this.matrix=new Ni,this.autoUpdate=!0,this.needsUpdate=!1,this._frustum=new No,this._frameExtents=new Y(1,1),this._viewportCount=1,this._viewports=[new Qr(0,0,1,1)]}getViewportCount(){return this._viewportCount}getFrustum(){return this._frustum}updateMatrices(e){let t=this.camera,n=this.matrix;lu.setFromMatrixPosition(e.matrixWorld),t.position.copy(lu),uu.setFromMatrixPosition(e.target.matrixWorld),t.lookAt(uu),t.updateMatrixWorld(),cu.multiplyMatrices(t.projectionMatrix,t.matrixWorldInverse),this._frustum.setFromProjectionMatrix(cu),n.set(.5,0,0,.5,0,.5,0,.5,0,0,.5,.5,0,0,0,1),n.multiply(cu)}getViewport(e){return this._viewports[e]}getFrameExtents(){return this._frameExtents}dispose(){this.map&&this.map.dispose(),this.mapPass&&this.mapPass.dispose()}copy(e){return this.camera=e.camera.clone(),this.bias=e.bias,this.radius=e.radius,this.mapSize.copy(e.mapSize),this}clone(){return new this.constructor().copy(this)}toJSON(){let e={};return this.bias!==0&&(e.bias=this.bias),this.normalBias!==0&&(e.normalBias=this.normalBias),this.radius!==1&&(e.radius=this.radius),(this.mapSize.x!==512||this.mapSize.y!==512)&&(e.mapSize=this.mapSize.toArray()),e.camera=this.camera.toJSON(!1).object,delete e.camera.matrix,e}},fu=class extends du{constructor(){super(new qo(-5,5,5,-5,.5,500)),this.isDirectionalLightShadow=!0}},pu=class extends su{constructor(e,t){super(e,t),this.isDirectionalLight=!0,this.type=`DirectionalLight`,this.position.copy(oa.DEFAULT_UP),this.updateMatrix(),this.target=new oa,this.shadow=new fu}dispose(){this.shadow.dispose()}copy(e){return super.copy(e),this.target=e.target.clone(),this.shadow=e.shadow.clone(),this}},mu=class extends su{constructor(e,t){super(e,t),this.isAmbientLight=!0,this.type=`AmbientLight`}},hu=`\\[\\]\\.:\\/`,gu=RegExp(`[`+hu+`]`,`g`),_u=`[^`+hu+`]`,vu=`[^`+hu.replace(`\\.`,``)+`]`,yu=`((?:WC+[\\/:])*)`.replace(`WC`,_u),bu=`(WCOD+)?`.replace(`WCOD`,vu),xu=`(?:\\.(WC+)(?:\\[(.+)\\])?)?`.replace(`WC`,_u),Su=`\\.(WC+)(?:\\[(.+)\\])?`.replace(`WC`,_u),Cu=RegExp(`^`+yu+bu+xu+Su+`$`),wu=[`material`,`materials`,`bones`,`map`],Tu=class{constructor(e,t,n){let r=n||Eu.parseTrackName(t);this._targetGroup=e,this._bindings=e.subscribe_(t,r)}getValue(e,t){this.bind();let n=this._targetGroup.nCachedObjects_,r=this._bindings[n];r!==void 0&&r.getValue(e,t)}setValue(e,t){let n=this._bindings;for(let r=this._targetGroup.nCachedObjects_,i=n.length;r!==i;++r)n[r].setValue(e,t)}bind(){let e=this._bindings;for(let t=this._targetGroup.nCachedObjects_,n=e.length;t!==n;++t)e[t].bind()}unbind(){let e=this._bindings;for(let t=this._targetGroup.nCachedObjects_,n=e.length;t!==n;++t)e[t].unbind()}},Eu=class e{constructor(t,n,r){this.path=n,this.parsedPath=r||e.parseTrackName(n),this.node=e.findNode(t,this.parsedPath.nodeName),this.rootNode=t,this.getValue=this._getValue_unbound,this.setValue=this._setValue_unbound}static create(t,n,r){return t&&t.isAnimationObjectGroup?new e.Composite(t,n,r):new e(t,n,r)}static sanitizeNodeName(e){return e.replace(/\s/g,`_`).replace(gu,``)}static parseTrackName(e){let t=Cu.exec(e);if(t===null)throw Error(`PropertyBinding: Cannot parse trackName: `+e);let n={nodeName:t[2],objectName:t[3],objectIndex:t[4],propertyName:t[5],propertyIndex:t[6]},r=n.nodeName&&n.nodeName.lastIndexOf(`.`);if(r!==void 0&&r!==-1){let e=n.nodeName.substring(r+1);wu.indexOf(e)!==-1&&(n.nodeName=n.nodeName.substring(0,r),n.objectName=e)}if(n.propertyName===null||n.propertyName.length===0)throw Error(`PropertyBinding: can not parse propertyName from trackName: `+e);return n}static findNode(e,t){if(t===void 0||t===``||t===`.`||t===-1||t===e.name||t===e.uuid)return e;if(e.skeleton){let n=e.skeleton.getBoneByName(t);if(n!==void 0)return n}if(e.children){let n=function(e){for(let r=0;r<e.length;r++){let i=e[r];if(i.name===t||i.uuid===t)return i;let a=n(i.children);if(a)return a}return null},r=n(e.children);if(r)return r}return null}_getValue_unavailable(){}_setValue_unavailable(){}_getValue_direct(e,t){e[t]=this.targetObject[this.propertyName]}_getValue_array(e,t){let n=this.resolvedProperty;for(let r=0,i=n.length;r!==i;++r)e[t++]=n[r]}_getValue_arrayElement(e,t){e[t]=this.resolvedProperty[this.propertyIndex]}_getValue_toArray(e,t){this.resolvedProperty.toArray(e,t)}_setValue_direct(e,t){this.targetObject[this.propertyName]=e[t]}_setValue_direct_setNeedsUpdate(e,t){this.targetObject[this.propertyName]=e[t],this.targetObject.needsUpdate=!0}_setValue_direct_setMatrixWorldNeedsUpdate(e,t){this.targetObject[this.propertyName]=e[t],this.targetObject.matrixWorldNeedsUpdate=!0}_setValue_array(e,t){let n=this.resolvedProperty;for(let r=0,i=n.length;r!==i;++r)n[r]=e[t++]}_setValue_array_setNeedsUpdate(e,t){let n=this.resolvedProperty;for(let r=0,i=n.length;r!==i;++r)n[r]=e[t++];this.targetObject.needsUpdate=!0}_setValue_array_setMatrixWorldNeedsUpdate(e,t){let n=this.resolvedProperty;for(let r=0,i=n.length;r!==i;++r)n[r]=e[t++];this.targetObject.matrixWorldNeedsUpdate=!0}_setValue_arrayElement(e,t){this.resolvedProperty[this.propertyIndex]=e[t]}_setValue_arrayElement_setNeedsUpdate(e,t){this.resolvedProperty[this.propertyIndex]=e[t],this.targetObject.needsUpdate=!0}_setValue_arrayElement_setMatrixWorldNeedsUpdate(e,t){this.resolvedProperty[this.propertyIndex]=e[t],this.targetObject.matrixWorldNeedsUpdate=!0}_setValue_fromArray(e,t){this.resolvedProperty.fromArray(e,t)}_setValue_fromArray_setNeedsUpdate(e,t){this.resolvedProperty.fromArray(e,t),this.targetObject.needsUpdate=!0}_setValue_fromArray_setMatrixWorldNeedsUpdate(e,t){this.resolvedProperty.fromArray(e,t),this.targetObject.matrixWorldNeedsUpdate=!0}_getValue_unbound(e,t){this.bind(),this.getValue(e,t)}_setValue_unbound(e,t){this.bind(),this.setValue(e,t)}bind(){let t=this.node,n=this.parsedPath,r=n.objectName,i=n.propertyName,a=n.propertyIndex;if(t||(t=e.findNode(this.rootNode,n.nodeName),this.node=t),this.getValue=this._getValue_unavailable,this.setValue=this._setValue_unavailable,!t){console.warn(`THREE.PropertyBinding: No target node found for track: `+this.path+`.`);return}if(r){let e=n.objectIndex;switch(r){case`materials`:if(!t.material){console.error(`THREE.PropertyBinding: Can not bind to material as node does not have a material.`,this);return}if(!t.material.materials){console.error(`THREE.PropertyBinding: Can not bind to material.materials as node.material does not have a materials array.`,this);return}t=t.material.materials;break;case`bones`:if(!t.skeleton){console.error(`THREE.PropertyBinding: Can not bind to bones as node does not have a skeleton.`,this);return}t=t.skeleton.bones;for(let n=0;n<t.length;n++)if(t[n].name===e){e=n;break}break;case`map`:if(`map`in t){t=t.map;break}if(!t.material){console.error(`THREE.PropertyBinding: Can not bind to material as node does not have a material.`,this);return}if(!t.material.map){console.error(`THREE.PropertyBinding: Can not bind to material.map as node.material does not have a map.`,this);return}t=t.material.map;break;default:if(t[r]===void 0){console.error(`THREE.PropertyBinding: Can not bind to objectName of node undefined.`,this);return}t=t[r]}if(e!==void 0){if(t[e]===void 0){console.error(`THREE.PropertyBinding: Trying to bind to objectIndex of objectName, but is undefined.`,this,t);return}t=t[e]}}let o=t[i];if(o===void 0){let e=n.nodeName;console.error(`THREE.PropertyBinding: Trying to update property for track: `+e+`.`+i+` but it wasn't found.`,t);return}let s=this.Versioning.None;this.targetObject=t,t.needsUpdate===void 0?t.matrixWorldNeedsUpdate!==void 0&&(s=this.Versioning.MatrixWorldNeedsUpdate):s=this.Versioning.NeedsUpdate;let c=this.BindingType.Direct;if(a!==void 0){if(i===`morphTargetInfluences`){if(!t.geometry){console.error(`THREE.PropertyBinding: Can not bind to morphTargetInfluences because node does not have a geometry.`,this);return}if(!t.geometry.morphAttributes){console.error(`THREE.PropertyBinding: Can not bind to morphTargetInfluences because node does not have a geometry.morphAttributes.`,this);return}t.morphTargetDictionary[a]!==void 0&&(a=t.morphTargetDictionary[a])}c=this.BindingType.ArrayElement,this.resolvedProperty=o,this.propertyIndex=a}else o.fromArray!==void 0&&o.toArray!==void 0?(c=this.BindingType.HasFromToArray,this.resolvedProperty=o):Array.isArray(o)?(c=this.BindingType.EntireArray,this.resolvedProperty=o):this.propertyName=i;this.getValue=this.GetterByBindingType[c],this.setValue=this.SetterByBindingTypeAndVersioning[c][s]}unbind(){this.node=null,this.getValue=this._getValue_unbound,this.setValue=this._setValue_unbound}};Eu.Composite=Tu,Eu.prototype.BindingType={Direct:0,EntireArray:1,ArrayElement:2,HasFromToArray:3},Eu.prototype.Versioning={None:0,NeedsUpdate:1,MatrixWorldNeedsUpdate:2},Eu.prototype.GetterByBindingType=[Eu.prototype._getValue_direct,Eu.prototype._getValue_array,Eu.prototype._getValue_arrayElement,Eu.prototype._getValue_toArray],Eu.prototype.SetterByBindingTypeAndVersioning=[[Eu.prototype._setValue_direct,Eu.prototype._setValue_direct_setNeedsUpdate,Eu.prototype._setValue_direct_setMatrixWorldNeedsUpdate],[Eu.prototype._setValue_array,Eu.prototype._setValue_array_setNeedsUpdate,Eu.prototype._setValue_array_setMatrixWorldNeedsUpdate],[Eu.prototype._setValue_arrayElement,Eu.prototype._setValue_arrayElement_setNeedsUpdate,Eu.prototype._setValue_arrayElement_setMatrixWorldNeedsUpdate],[Eu.prototype._setValue_fromArray,Eu.prototype._setValue_fromArray_setNeedsUpdate,Eu.prototype._setValue_fromArray_setMatrixWorldNeedsUpdate]];var Du=class{constructor(e=1,t=0,n=0){return this.radius=e,this.phi=t,this.theta=n,this}set(e,t,n){return this.radius=e,this.phi=t,this.theta=n,this}copy(e){return this.radius=e.radius,this.phi=e.phi,this.theta=e.theta,this}makeSafe(){let e=1e-6;return this.phi=Math.max(e,Math.min(Math.PI-e,this.phi)),this}setFromVector3(e){return this.setFromCartesianCoords(e.x,e.y,e.z)}setFromCartesianCoords(e,t,n){return this.radius=Math.sqrt(e*e+t*t+n*n),this.radius===0?(this.theta=0,this.phi=0):(this.theta=Math.atan2(e,n),this.phi=Math.acos(ur(t/this.radius,-1,1))),this}clone(){return new this.constructor().copy(this)}};typeof __THREE_DEVTOOLS__<`u`&&__THREE_DEVTOOLS__.dispatchEvent(new CustomEvent(`register`,{detail:{revision:`162`}})),typeof window<`u`&&(window.__THREE__?console.warn(`WARNING: Multiple instances of Three.js being imported.`):window.__THREE__=`162`);function Ou(e,t,n=2){let r=t&&t.length,i=r?t[0]*n:e.length,a=ku(e,0,i,n,!0),o=[];if(!a||a.next===a.prev)return o;let s,c,l;if(r&&(a=Iu(e,t,a,n)),e.length>80*n){s=e[0],c=e[1];let t=s,r=c;for(let a=n;a<i;a+=n){let n=e[a],i=e[a+1];n<s&&(s=n),i<c&&(c=i),n>t&&(t=n),i>r&&(r=i)}l=Math.max(t-s,r-c),l=l===0?0:32767/l}return ju(a,o,n,s,c,l,0),o}function ku(e,t,n,r,i){let a;if(i===od(e,t,n,r)>0)for(let i=t;i<n;i+=r)a=rd(i/r|0,e[i],e[i+1],a);else for(let i=n-r;i>=t;i-=r)a=rd(i/r|0,e[i],e[i+1],a);return a&&Yu(a,a.next)&&(id(a),a=a.next),a}function Au(e,t){if(!e)return e;t||=e;let n=e,r;do if(r=!1,!n.steiner&&(Yu(n,n.next)||Ju(n.prev,n,n.next)===0)){if(id(n),n=t=n.prev,n===n.next)break;r=!0}else n=n.next;while(r||n!==t);return t}function ju(e,t,n,r,i,a,o){if(!e)return;!o&&a&&Vu(e,r,i,a);let s=e;for(;e.prev!==e.next;){let c=e.prev,l=e.next;if(a?Nu(e,r,i,a):Mu(e)){t.push(c.i,e.i,l.i),id(e),e=l.next,s=l.next;continue}if(e=l,e===s){o?o===1?(e=Pu(Au(e),t),ju(e,t,n,r,i,a,2)):o===2&&Fu(e,t,n,r,i,a):ju(Au(e),t,n,r,i,a,1);break}}}function Mu(e){let t=e.prev,n=e,r=e.next;if(Ju(t,n,r)>=0)return!1;let i=t.x,a=n.x,o=r.x,s=t.y,c=n.y,l=r.y,u=Math.min(i,a,o),d=Math.min(s,c,l),f=Math.max(i,a,o),p=Math.max(s,c,l),m=r.next;for(;m!==t;){if(m.x>=u&&m.x<=f&&m.y>=d&&m.y<=p&&Ku(i,s,a,c,o,l,m.x,m.y)&&Ju(m.prev,m,m.next)>=0)return!1;m=m.next}return!0}function Nu(e,t,n,r){let i=e.prev,a=e,o=e.next;if(Ju(i,a,o)>=0)return!1;let s=i.x,c=a.x,l=o.x,u=i.y,d=a.y,f=o.y,p=Math.min(s,c,l),m=Math.min(u,d,f),h=Math.max(s,c,l),g=Math.max(u,d,f),_=Uu(p,m,t,n,r),v=Uu(h,g,t,n,r),y=e.prevZ,b=e.nextZ;for(;y&&y.z>=_&&b&&b.z<=v;){if(y.x>=p&&y.x<=h&&y.y>=m&&y.y<=g&&y!==i&&y!==o&&Ku(s,u,c,d,l,f,y.x,y.y)&&Ju(y.prev,y,y.next)>=0||(y=y.prevZ,b.x>=p&&b.x<=h&&b.y>=m&&b.y<=g&&b!==i&&b!==o&&Ku(s,u,c,d,l,f,b.x,b.y)&&Ju(b.prev,b,b.next)>=0))return!1;b=b.nextZ}for(;y&&y.z>=_;){if(y.x>=p&&y.x<=h&&y.y>=m&&y.y<=g&&y!==i&&y!==o&&Ku(s,u,c,d,l,f,y.x,y.y)&&Ju(y.prev,y,y.next)>=0)return!1;y=y.prevZ}for(;b&&b.z<=v;){if(b.x>=p&&b.x<=h&&b.y>=m&&b.y<=g&&b!==i&&b!==o&&Ku(s,u,c,d,l,f,b.x,b.y)&&Ju(b.prev,b,b.next)>=0)return!1;b=b.nextZ}return!0}function Pu(e,t){let n=e;do{let r=n.prev,i=n.next.next;!Yu(r,i)&&Xu(r,n,n.next,i)&&ed(r,i)&&ed(i,r)&&(t.push(r.i,n.i,i.i),id(n),id(n.next),n=e=i),n=n.next}while(n!==e);return Au(n)}function Fu(e,t,n,r,i,a){let o=e;do{let e=o.next.next;for(;e!==o.prev;){if(o.i!==e.i&&qu(o,e)){let s=nd(o,e);o=Au(o,o.next),s=Au(s,s.next),ju(o,t,n,r,i,a,0),ju(s,t,n,r,i,a,0);return}e=e.next}o=o.next}while(o!==e)}function Iu(e,t,n,r){let i=[];for(let n=0,a=t.length;n<a;n++){let o=ku(e,t[n]*r,n<a-1?t[n+1]*r:e.length,r,!1);o===o.next&&(o.steiner=!0),i.push(Wu(o))}i.sort(Lu);for(let e=0;e<i.length;e++)n=Ru(i[e],n);return n}function Lu(e,t){let n=e.x-t.x;return n===0&&(n=e.y-t.y,n===0&&(n=(e.next.y-e.y)/(e.next.x-e.x)-(t.next.y-t.y)/(t.next.x-t.x))),n}function Ru(e,t){let n=zu(e,t);if(!n)return t;let r=nd(n,e);return Au(r,r.next),Au(n,n.next)}function zu(e,t){let n=t,r=e.x,i=e.y,a=-1/0,o;if(Yu(e,n))return n;do{if(Yu(e,n.next))return n.next;if(i<=n.y&&i>=n.next.y&&n.next.y!==n.y){let e=n.x+(i-n.y)*(n.next.x-n.x)/(n.next.y-n.y);if(e<=r&&e>a&&(a=e,o=n.x<n.next.x?n:n.next,e===r))return o}n=n.next}while(n!==t);if(!o)return null;let s=o,c=o.x,l=o.y,u=1/0;n=o;do{if(r>=n.x&&n.x>=c&&r!==n.x&&Gu(i<l?r:a,i,c,l,i<l?a:r,i,n.x,n.y)){let t=Math.abs(i-n.y)/(r-n.x);ed(n,e)&&(t<u||t===u&&(n.x>o.x||n.x===o.x&&Bu(o,n)))&&(o=n,u=t)}n=n.next}while(n!==s);return o}function Bu(e,t){return Ju(e.prev,e,t.prev)<0&&Ju(t.next,e,e.next)<0}function Vu(e,t,n,r){let i=e;do i.z===0&&(i.z=Uu(i.x,i.y,t,n,r)),i.prevZ=i.prev,i.nextZ=i.next,i=i.next;while(i!==e);i.prevZ.nextZ=null,i.prevZ=null,Hu(i)}function Hu(e){let t,n=1;do{let r=e,i;e=null;let a=null;for(t=0;r;){t++;let o=r,s=0;for(let e=0;e<n&&(s++,o=o.nextZ,o);e++);let c=n;for(;s>0||c>0&&o;)s!==0&&(c===0||!o||r.z<=o.z)?(i=r,r=r.nextZ,s--):(i=o,o=o.nextZ,c--),a?a.nextZ=i:e=i,i.prevZ=a,a=i;r=o}a.nextZ=null,n*=2}while(t>1);return e}function Uu(e,t,n,r,i){return e=(e-n)*i|0,t=(t-r)*i|0,e=(e|e<<8)&16711935,e=(e|e<<4)&252645135,e=(e|e<<2)&858993459,e=(e|e<<1)&1431655765,t=(t|t<<8)&16711935,t=(t|t<<4)&252645135,t=(t|t<<2)&858993459,t=(t|t<<1)&1431655765,e|t<<1}function Wu(e){let t=e,n=e;do(t.x<n.x||t.x===n.x&&t.y<n.y)&&(n=t),t=t.next;while(t!==e);return n}function Gu(e,t,n,r,i,a,o,s){return(i-o)*(t-s)>=(e-o)*(a-s)&&(e-o)*(r-s)>=(n-o)*(t-s)&&(n-o)*(a-s)>=(i-o)*(r-s)}function Ku(e,t,n,r,i,a,o,s){return!(e===o&&t===s)&&Gu(e,t,n,r,i,a,o,s)}function qu(e,t){return e.next.i!==t.i&&e.prev.i!==t.i&&!$u(e,t)&&(ed(e,t)&&ed(t,e)&&td(e,t)&&(Ju(e.prev,e,t.prev)||Ju(e,t.prev,t))||Yu(e,t)&&Ju(e.prev,e,e.next)>0&&Ju(t.prev,t,t.next)>0)}function Ju(e,t,n){return(t.y-e.y)*(n.x-t.x)-(t.x-e.x)*(n.y-t.y)}function Yu(e,t){return e.x===t.x&&e.y===t.y}function Xu(e,t,n,r){let i=Qu(Ju(e,t,n)),a=Qu(Ju(e,t,r)),o=Qu(Ju(n,r,e)),s=Qu(Ju(n,r,t));return!!(i!==a&&o!==s||i===0&&Zu(e,n,t)||a===0&&Zu(e,r,t)||o===0&&Zu(n,e,r)||s===0&&Zu(n,t,r))}function Zu(e,t,n){return t.x<=Math.max(e.x,n.x)&&t.x>=Math.min(e.x,n.x)&&t.y<=Math.max(e.y,n.y)&&t.y>=Math.min(e.y,n.y)}function Qu(e){return e>0?1:e<0?-1:0}function $u(e,t){let n=e;do{if(n.i!==e.i&&n.next.i!==e.i&&n.i!==t.i&&n.next.i!==t.i&&Xu(n,n.next,e,t))return!0;n=n.next}while(n!==e);return!1}function ed(e,t){return Ju(e.prev,e,e.next)<0?Ju(e,t,e.next)>=0&&Ju(e,e.prev,t)>=0:Ju(e,t,e.prev)<0||Ju(e,e.next,t)<0}function td(e,t){let n=e,r=!1,i=(e.x+t.x)/2,a=(e.y+t.y)/2;do n.y>a!=n.next.y>a&&n.next.y!==n.y&&i<(n.next.x-n.x)*(a-n.y)/(n.next.y-n.y)+n.x&&(r=!r),n=n.next;while(n!==e);return r}function nd(e,t){let n=ad(e.i,e.x,e.y),r=ad(t.i,t.x,t.y),i=e.next,a=t.prev;return e.next=t,t.prev=e,n.next=i,i.prev=n,r.next=n,n.prev=r,a.next=r,r.prev=a,r}function rd(e,t,n,r){let i=ad(e,t,n);return r?(i.next=r.next,i.prev=r,r.next.prev=i,r.next=i):(i.prev=i,i.next=i),i}function id(e){e.next.prev=e.prev,e.prev.next=e.next,e.prevZ&&(e.prevZ.nextZ=e.nextZ),e.nextZ&&(e.nextZ.prevZ=e.prevZ)}function ad(e,t,n){return{i:e,x:t,y:n,prev:null,next:null,z:0,prevZ:null,nextZ:null,steiner:!1}}function od(e,t,n,r){let i=0;for(let a=t,o=n-r;a<n;a+=r)i+=(e[o]-e[a])*(e[a+1]+e[o+1]),o=a;return i}var sd,cd=new Map;function ld(){return sd||(sd=new Vl({antialias:!0,alpha:!0}),sd.setPixelRatio(window.devicePixelRatio),sd.setSize(window.innerWidth,window.innerHeight),sd.setClearColor(0,0),sd)}function ud(){sd&&(sd.setClearColor(0,0),sd.setScissorTest(!1),sd.clear(),sd.setScissorTest(!0),cd.forEach(e=>{let t=e.canvas.getBoundingClientRect();if(t.bottom<0||t.top>sd.domElement.clientHeight||t.right<0||t.left>sd.domElement.clientWidth)return;let n=t.right-t.left,r=t.bottom-t.top,i=t.left,a=sd.domElement.clientHeight-t.bottom;sd.setViewport(i,a,n,r),sd.setScissor(i,a,n,r),e.camera.aspect=n/r,e.camera.updateProjectionMatrix(),sd.render(e.scene,e.camera)}))}var dd=e=>{let t=[],n=[],r=[];for(let i of e.split(`
`)){let e=i.trim().split(` `);switch(e.shift()){case`v`:for(;e.length>=3;)e.length>=6&&e.splice(0,3),t.push([parseFloat(e.shift()),parseFloat(e.shift()),parseFloat(e.shift())]);break;case`t`:for(;e.length>=3;)n.push([parseInt(e.shift())-1,parseInt(e.shift())-1,parseInt(e.shift())-1]);break;case`f`:{let t=[];for(;e.length>=1;)t.push(parseInt(e.shift())-1);t.length>0&&r.push([t]);break}case`h`:{let t=[];for(;e.length>=1;)t.push(parseInt(e.shift())-1);r.length>0&&t.length>0&&r.at(-1).push(t);break}}}return{vertices:t,triangles:n,faces:r}},fd=new Ni,pd=e=>{if(!e)return fd;if(typeof e==`string`){let t=e.split(` `),n=t.shift(),r=new Ni;switch(n){case`x`:case`y`:case`z`:t.shift();let e=Math.PI*2*parseFloat(t.shift());return n===`x`?r.makeRotationX(e):n===`y`?r.makeRotationY(e):r.makeRotationZ(e),r;case`s`:return t.splice(0,3),r.makeScale(parseFloat(t.shift()),parseFloat(t.shift()),parseFloat(t.shift()));case`t`:return t.splice(0,3),r.makeTranslation(parseFloat(t.shift()),parseFloat(t.shift()),parseFloat(t.shift()));case`m`:t.splice(0,13);let i=t.map(parseFloat);return r.set(i[0],i[1],i[2],i[3],i[4],i[5],i[6],i[7],i[8],i[9],i[10],i[11],0,0,0,i[12]),r;default:return fd}}else if(Array.isArray(e)){let t=pd(e[1]);return e[0]===`i`?t.clone().invert():pd(e[0]).clone().multiply(t)}return fd},md=async({assets:e,shape:t,scene:n})=>{let r=async(t,i=fd)=>{let a=i.clone().multiply(pd(t.tf));if(t.geometry){let r=await e.getText(t.geometry);if(r){let{vertices:e,triangles:t,faces:i}=dd(r),o=new Wl({side:2});if(t.length>0){let r=[],i=[];for(let n of t){let t=e[n[0]],a=e[n[1]],o=e[n[2]];if(!t||!a||!o)continue;let s=new Z(...t),c=new Z(...a),l=new Z(...o),u=new Z().crossVectors(new Z().subVectors(l,c),new Z().subVectors(s,c)).normalize();r.push(s.x,s.y,s.z,c.x,c.y,c.z,l.x,l.y,l.z);for(let e=0;e<3;e++)i.push(u.x,u.y,u.z)}if(r.length>0){let e=new Ba;e.setAttribute(`position`,new Ma(r,3)),e.setAttribute(`normal`,new Ma(i,3));let t=new io(e,o);t.applyMatrix4(a),n.add(t)}}for(let[t,...r]of i){let i=t.map(t=>e[t]).filter(e=>e!==void 0);if(i.length<3)continue;let s=r.map(t=>t.map(t=>e[t]).filter(e=>e!==void 0)),c=new Z;c.crossVectors(new Z().subVectors(new Z(...i[2]),new Z(...i[1])),new Z().subVectors(new Z(...i[0]),new Z(...i[1]))).normalize();let l=0,u=1,d=Math.abs(c.x),f=Math.abs(c.y),p=Math.abs(c.z);d>=f&&d>=p?(l=1,u=2):f>=d&&f>=p&&(l=0,u=2);let m=[],h=[];for(let e of i)m.push(e[l],e[u]);for(let e of s){h.push(m.length/2);for(let t of e)m.push(t[l],t[u])}let g=Ou(m,h),_=[...i,...s.flat()],v=_.flatMap(e=>[e[0],e[1],e[2]]),y=_.flatMap(()=>[c.x,c.y,c.z]),b=new Ba;b.setAttribute(`position`,new Ma(v,3)),b.setAttribute(`normal`,new Ma(y,3)),b.setIndex(g);let x=new io(b,o);x.applyMatrix4(a),n.add(x)}}}if(t.shapes)for(let e of t.shapes)await r(e,a)};await r(t)},hd=class{constructor(e){this.vfs=e,this.cache=new Map,this.main=null}async parse(e){let t=0;for(e[0]===`
`&&t++;t<e.length&&e[t]===`=`;){t++;let n=e.indexOf(`
`,t),r=e.substring(t,n);t=n+1;let i=r.indexOf(` `),a=parseInt(r.substring(0,i),10),o=r.substring(i+1),s=e.substring(t,t+a);t+=a,e[t]===`
`&&t++,o.startsWith(`files/`)?this.main=JSON.parse(s):o.startsWith(`assets/text/`)&&this.cache.set(o.substring(12),s)}return this.main}async getText(e){if(this.cache.has(e))return this.cache.get(e);if(!this.vfs)return null;try{if(e.startsWith(`vfs:/`)){let t=new URL(e.replace(`vfs:/`,`vfs://`)),n=t.pathname.slice(1),r=Object.fromEntries(t.searchParams.entries()),i=await this.vfs.readData(n,r);if(i)return this.cache.set(e,i),i}let t=await this.vfs.readData(`geo/mesh`,{id:e});if(t)return this.cache.set(e,t),t}catch(t){console.warn(`[Renderer] Asset resolution failed:`,e,t)}return null}};async function gd(e,t,n){let r=new hd(e),i;if(typeof t==`string`){let e=t.trim();if(e.startsWith(`=`)||e.startsWith(`
=`))i=await r.parse(t);else if(e.includes(`v `)){let e=`raw-`+Math.random().toString(36).slice(2);r.cache.set(e,t),i={geometry:e}}}else typeof t==`object`&&(i=t);if(!i)throw Error(`Invalid data`);return await md({assets:r,shape:i,scene:n}),i}var _d=new Map;async function vd(e,t,n=256,r=256){let i=typeof t==`string`?t.length+t.slice(0,100):JSON.stringify(t).length;if(_d.has(i))return _d.get(i);sd||ld();let a=new Ul;try{await gd(e,t,a)}catch(e){return console.warn(`[Thumbnail] Render failed:`,e),null}if(a.children.length===0)return null;let o=new oi().setFromObject(a),s=o.getCenter(new Z),c=o.getSize(new Z),l=Math.max(c.x,c.y,c.z)||1,u=new xo(45,n/r,.1,1e4);u.position.set(s.x,s.y,s.z+l*2),u.up.set(0,1,0),u.lookAt(s),a.add(new mu(16777215,.7));let d=new pu(16777215,.8);d.position.set(l,l*2,l),a.add(d);let f=new ei(n,r);sd.setRenderTarget(f),sd.render(a,u);let p=new Uint8Array(n*r*4);sd.readRenderTargetPixels(f,0,0,n,r,p),sd.setRenderTarget(null);let m=document.createElement(`canvas`);m.width=n,m.height=r;let h=m.getContext(`2d`),g=h.createImageData(n,r);for(let e=0;e<r;e++)for(let t=0;t<n;t++){let i=(e*n+t)*4,a=((r-e-1)*n+t)*4;for(let e=0;e<4;e++)g.data[a+e]=p[i+e]}h.putImageData(g,0,0);let _=m.toDataURL(`image/png`);return _d.set(i,_),_}var yd={xmlns:`http://www.w3.org/2000/svg`,width:24,height:24,viewBox:`0 0 24 24`,fill:`none`,stroke:`currentColor`,"stroke-width":2,"stroke-linecap":`round`,"stroke-linejoin":`round`},bd=K(`<svg>`),xd=e=>e.replace(/([a-z0-9])([A-Z])/g,`$1-$2`).toLowerCase(),Sd=e=>{let[t,n]=Oe(e,[`color`,`size`,`strokeWidth`,`children`,`class`,`name`,`iconNode`,`absoluteStrokeWidth`]);return(()=>{var e=bd();return Qe(e,De(yd,{get width(){return t.size??yd.width},get height(){return t.size??yd.height},get stroke(){return t.color??yd.stroke},get"stroke-width"(){return Be(()=>!!t.absoluteStrokeWidth)()?Number(t.strokeWidth??yd[`stroke-width`])*24/Number(t.size):Number(t.strokeWidth??yd[`stroke-width`])},get class(){return`lucide lucide-${xd(t?.name??`icon`)} ${t.class==null?``:t.class}`}},n),!0,!0),J(e,G(Ae,{get each(){return t.iconNode},children:([e,t])=>G(ht,De({component:e},t))})),e})()},Cd=[[`path`,{d:`M22 12h-4l-3 9L9 3l-3 9H2`,key:`d5dnw9`}]],wd=e=>G(Sd,De(e,{name:`Activity`,iconNode:Cd})),Td=[[`path`,{d:`M21 8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16Z`,key:`hh9hay`}],[`path`,{d:`m3.3 7 8.7 5 8.7-5`,key:`g66t2b`}],[`path`,{d:`M12 22V12`,key:`d0xqtd`}]],Ed=e=>G(Sd,De(e,{name:`Box`,iconNode:Td})),Dd=[[`path`,{d:`M20 6 9 17l-5-5`,key:`1gmf2c`}]],Od=e=>G(Sd,De(e,{name:`Check`,iconNode:Dd})),kd=[[`circle`,{cx:`12`,cy:`12`,r:`10`,key:`1mglay`}],[`path`,{d:`m9 12 2 2 4-4`,key:`dzmm74`}]],Ad=e=>G(Sd,De(e,{name:`CircleCheck`,iconNode:kd})),jd=[[`circle`,{cx:`12`,cy:`12`,r:`10`,key:`1mglay`}],[`polyline`,{points:`12 6 12 12 16 14`,key:`68esgv`}]],Md=e=>G(Sd,De(e,{name:`Clock`,iconNode:jd})),Nd=[[`rect`,{width:`16`,height:`16`,x:`4`,y:`4`,rx:`2`,key:`14l7u7`}],[`rect`,{width:`6`,height:`6`,x:`9`,y:`9`,rx:`1`,key:`5aljv4`}],[`path`,{d:`M15 2v2`,key:`13l42r`}],[`path`,{d:`M15 20v2`,key:`15mkzm`}],[`path`,{d:`M2 15h2`,key:`1gxd5l`}],[`path`,{d:`M2 9h2`,key:`1bbxkp`}],[`path`,{d:`M20 15h2`,key:`19e6y8`}],[`path`,{d:`M20 9h2`,key:`19tzq7`}],[`path`,{d:`M9 2v2`,key:`165o2o`}],[`path`,{d:`M9 20v2`,key:`i2bqo8`}]],Pd=e=>G(Sd,De(e,{name:`Cpu`,iconNode:Nd})),Fd=[[`ellipse`,{cx:`12`,cy:`5`,rx:`9`,ry:`3`,key:`msslwz`}],[`path`,{d:`M3 5V19A9 3 0 0 0 21 19V5`,key:`1wlel7`}],[`path`,{d:`M3 12A9 3 0 0 0 21 12`,key:`mv7ke4`}]],Id=e=>G(Sd,De(e,{name:`Database`,iconNode:Fd})),Ld=[[`path`,{d:`M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z`,key:`1rqfz7`}],[`path`,{d:`M14 2v4a2 2 0 0 0 2 2h4`,key:`tnqrlb`}],[`path`,{d:`M10 12a1 1 0 0 0-1 1v1a1 1 0 0 1-1 1 1 1 0 0 1 1 1v1a1 1 0 0 0 1 1`,key:`1oajmo`}],[`path`,{d:`M14 18a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1 1 1 0 0 1-1-1v-1a1 1 0 0 0-1-1`,key:`mpwhp6`}]],Rd=e=>G(Sd,De(e,{name:`FileJson`,iconNode:Ld})),zd=[[`circle`,{cx:`12`,cy:`12`,r:`10`,key:`1mglay`}],[`path`,{d:`M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20`,key:`13o1zl`}],[`path`,{d:`M2 12h20`,key:`9i4pu4`}]],Bd=e=>G(Sd,De(e,{name:`Globe`,iconNode:zd})),Vd=[[`path`,{d:`M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z`,key:`yt0hxn`}]],Hd=e=>G(Sd,De(e,{name:`Hexagon`,iconNode:Vd})),Ud=[[`path`,{d:`m12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83Z`,key:`8b97xw`}],[`path`,{d:`m22 17.65-9.17 4.16a2 2 0 0 1-1.66 0L2 17.65`,key:`dd6zsq`}],[`path`,{d:`m22 12.65-9.17 4.16a2 2 0 0 1-1.66 0L2 12.65`,key:`ep9fru`}]],Wd=e=>G(Sd,De(e,{name:`Layers`,iconNode:Ud})),Gd=[[`path`,{d:`M3 12a9 9 0 0 1 9-9 9.75 9.75 0 0 1 6.74 2.74L21 8`,key:`v9h5vc`}],[`path`,{d:`M21 3v5h-5`,key:`1q7to0`}],[`path`,{d:`M21 12a9 9 0 0 1-9 9 9.75 9.75 0 0 1-6.74-2.74L3 16`,key:`3uifl3`}],[`path`,{d:`M8 16H3v5`,key:`1cv678`}]],Kd=e=>G(Sd,De(e,{name:`RefreshCw`,iconNode:Gd})),qd=[[`path`,{d:`M13.73 4a2 2 0 0 0-3.46 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z`,key:`14u9p9`}]],Jd=e=>G(Sd,De(e,{name:`Triangle`,iconNode:qd})),Yd=K(`<div class="absolute select-none p-4 rounded-xl border-2 border-white/20 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-hidden flex flex-col gap-3"style=width:400px;z-index:20><div class="flex justify-between items-center"><div class="text-xs font-black uppercase tracking-tighter text-white/40">Jot Engine Expression</div><div></div></div><textarea class="bg-black/40 border border-white/5 rounded-lg p-3 font-mono text-[11px] h-32 focus:outline-none focus:border-cyan-400/50 text-cyan-200 resize-none custom-scrollbar"></textarea><div class="flex gap-2"><button>`),Xd=`// JotCAD Expressions
hexagon({ radius: 30, variant: 'full' })`,Zd=e=>{let t,[n,r]=M({x:400,y:400}),[i,a]=M(Xd),[o,s]=M(!1),c=new Qt,l=new $t;te(()=>{(0,It.default)(t).draggable({listeners:{move(e){r({x:n().x+e.dx,y:n().y+e.dy})}}})});let u=async()=>{if(!o()){s(!0);try{console.log(`[JotNode] Compiling expression...`);let e=c.parse(i()),t=l.resolve(e);console.log(`[JotNode] Translated to selector:`,JSON.stringify(t,null,2));let n=await tn.read(t.path,t.parameters);if(!n)throw Error(`Mesh resolution failed: No provider found for `+t.path);let r=await n.getReader().read(),a=new TextDecoder,o=r.value?a.decode(r.value):`Empty result`;await tn.writeData(`ui/result/jot_eval`,{},r.value||new Uint8Array),console.log(`[JotNode] Result saved to ui/result/jot_eval. Content:`,o)}catch(e){console.error(`[JotNode] Compilation/Evaluation failed:`,e),alert(`Error: `+e.message)}finally{s(!1)}}};return(()=>{var e=Yd(),r=e.firstChild,s=r.firstChild.nextSibling,c=r.nextSibling,l=c.nextSibling.firstChild,d=t;return typeof d==`function`?$e(d,e):t=e,c.$$input=e=>a(e.target.value),Ge(c,`spellcheck`,!1),l.$$click=u,J(l,()=>o()?`EVALUATING...`:`EVALUATE JOT`),N(t=>{var r=`translate(${n().x}px, ${n().y}px)`,i=`w-2 h-2 rounded-full ${o()?`bg-cyan-400 animate-pulse`:`bg-white/10`}`,a=o(),c=`flex-1 py-2 rounded-lg text-sm font-bold transition-all ${o()?`bg-cyan-500/20 text-cyan-400`:`bg-cyan-400 text-black hover:bg-cyan-300`}`;return r!==t.e&&Ze(e,`transform`,t.e=r),i!==t.t&&Je(s,t.t=i),a!==t.a&&(l.disabled=t.a=a),c!==t.o&&Je(l,t.o=c),t},{e:void 0,t:void 0,a:void 0,o:void 0}),N(()=>c.value=i()),e})()};We([`input`,`click`]);var Qd=K(`<div class="w-2 h-2 rounded-full bg-cyan-400 animate-pulse">`),$d=K(`<div class="w-2 h-2 rounded-full bg-red-500"title="Discovery Error">`),ef=K(`<div class="text-[10px] text-white/20 italic p-2 text-center">No providers discovered.`),tf=K(`<div class="text-[10px] text-red-400 italic p-2 text-center">Discovery failed!`),nf=K(`<div class="absolute top-4 right-4 z-40 w-72 max-h-[80vh] flex flex-col gap-2 p-4 rounded-xl border border-white/10 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-hidden"><div class="flex justify-between items-center mb-1"><div class="flex items-center gap-2 text-white/60"><span class="text-xs font-black uppercase tracking-widest">Provider Catalog</span></div><button></button></div><div class="flex-1 overflow-y-auto custom-scrollbar flex flex-col gap-1">`),rf=K(`<div class="text-[10px] font-mono text-cyan-200/70 py-1.5 px-2 hover:bg-white/5 rounded cursor-pointer transition-colors flex justify-between items-center group"><span class=truncate></span><span class="text-white/20 text-[8px]"> params`),af=e=>{let t=()=>Object.entries(yn.schemas()||{}),[n,r]=M(!1),i=()=>yn.discoveryStatus(),a=async()=>{r(!0),await yn.discoverSchemas(),r(!1)};return(()=>{var e=nf(),r=e.firstChild,o=r.firstChild,s=o.firstChild,c=o.nextSibling,l=r.nextSibling;return J(o,G(Id,{size:16}),s),J(o,G(je,{get when(){return i()===`loading`},get children(){return Qd()}}),null),J(o,G(je,{get when(){return i()===`error`},get children(){return $d()}}),null),c.$$click=a,J(c,G(Kd,{size:14})),J(l,G(Ae,{get each(){return t()},children:([e,t])=>(()=>{var n=rf(),r=n.firstChild,i=r.nextSibling,a=i.firstChild;return J(r,e),J(i,()=>Object.keys(t).length,a),n})()}),null),J(l,G(je,{get when(){return i()===`empty`},get children(){return ef()}}),null),J(l,G(je,{get when(){return i()===`error`},get children(){return tf()}}),null),N(()=>Je(c,`text-white/40 hover:text-white transition-all ${n()?`animate-spin`:``}`)),e})()};We([`click`]);var of=K(`<div class="p-4 text-center text-white/20 text-[10px] uppercase italic text-balance">Silent Mesh...`),sf=K(`<div class="fixed flex flex-col gap-2 z-50 pointer-events-auto select-none overflow-hidden"><div class="p-3 rounded-t-xl bg-black/60 backdrop-blur-xl border-x-2 border-t-2 border-white/10 flex justify-between items-center cursor-move"><span class="text-[10px] font-black uppercase tracking-widest text-white/40">Mesh Pulse</span><div></div></div><div class="flex-1 overflow-y-auto p-2 bg-black/40 backdrop-blur-md border-x-2 border-b-2 border-white/10 rounded-b-xl flex flex-col gap-1 custom-scrollbar">`),cf=K(`<div class="group flex flex-col p-2 rounded-lg bg-white/5 border border-white/5 hover:border-white/10 transition-colors shrink-0"><div class="flex justify-between items-start gap-2"><span></span><span class="text-[8px] font-mono text-white/20"></span></div><div class="text-[11px] font-mono text-white/70 truncate"></div><div class="text-[9px] text-white/30 truncate uppercase tracking-tighter">`);function lf(){let e,t=yn.pulse,[n,r]=M({x:window.innerWidth-340,y:window.innerHeight-450}),[i,a]=M({width:320,height:400});te(()=>{(0,It.default)(e).draggable({listeners:{move(e){r(t=>({x:t.x+e.dx,y:t.y+e.dy}))}}}).resizable({edges:{top:!0,left:!0,bottom:!1,right:!1},listeners:{move(e){a(t=>({width:e.rect.width,height:e.rect.height})),r(t=>({x:e.rect.left,y:e.rect.top}))}}})});let o=e=>{switch(e){case`READ_START`:return`text-blue-400`;case`HANDLER_START`:return`text-yellow-400`;case`WORKING`:return`text-orange-400 animate-pulse`;case`SUCCESS`:return`text-green-400`;case`ERROR`:return`text-red-400`;default:return`text-white/50`}},s=e=>{try{if(typeof e==`object`)return`${e.path} (${Object.keys(e.parameters||{}).length} params)`;let[t,n]=e.split(`?`);return n?`${t} (...)`:t}catch{return String(e)}};return(()=>{var r=sf(),a=r.firstChild,c=a.firstChild.nextSibling,l=a.nextSibling,u=e;return typeof u==`function`?$e(u,r):e=r,J(l,G(je,{get when(){return t().length===0},get children(){return of()}}),null),J(l,G(Ae,{get each(){return[...t()].reverse()},children:e=>(()=>{var t=cf(),n=t.firstChild,r=n.firstChild,i=r.nextSibling,a=n.nextSibling,c=a.nextSibling;return J(r,()=>e.payload.type),J(i,()=>new Date(e.t).toLocaleTimeString([],{hour12:!1,hour:`2-digit`,minute:`2-digit`,second:`2-digit`})),J(a,()=>s(e.selector||e.topic)),J(c,()=>e.payload.peer||`unknown-peer`),N(t=>{var n=`text-[10px] font-bold ${o(e.payload.type)}`,i=JSON.stringify(e.selector);return n!==t.e&&Je(r,t.e=n),i!==t.t&&Ge(a,`title`,t.t=i),t},{e:void 0,t:void 0}),t})()}),null),N(e=>{var t=`${n().x}px`,a=`${n().y}px`,o=`${i().width}px`,s=`${i().height}px`,l=`w-2 h-2 rounded-full ${yn.isConnected()?`bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]`:`bg-red-500`}`;return t!==e.e&&Ze(r,`left`,e.e=t),a!==e.t&&Ze(r,`top`,e.t=a),o!==e.a&&Ze(r,`width`,e.a=o),s!==e.o&&Ze(r,`height`,e.o=s),l!==e.i&&Je(c,e.i=l),e},{e:void 0,t:void 0,a:void 0,o:void 0,i:void 0}),r})()}var uf=K(`<div class="fixed inset-0 pointer-events-none z-0 overflow-hidden"><svg class="w-full h-full">`),df=K(`<svg><line stroke=white stroke-width=1 stroke-opacity=0.1></svg>`,!1,!0,!1),ff=K(`<svg><circle r=3 fill=#f97316 class=animate-ping><animateMotion dur=0.4s repeatCount=1></svg>`,!1,!0,!1),pf=K(`<svg><text y=45 text-anchor=middle fill=#f97316 class="text-[8px] font-bold font-mono"> PPS</svg>`,!1,!0,!1),mf=K(`<svg><g class="pointer-events-auto cursor-grab active:cursor-grabbing"><circle r=35></circle><circle r=25 fill=#000 fill-opacity=0.8 stroke-width=2 stroke-opacity=0.4></circle><foreignObject x=-15 y=-15 width=30 height=30><div class="w-full h-full flex items-center justify-center text-white/40"></div></foreignObject><g transform="translate(0, 32)"><rect x=-12 y=-6 width=24 height=12 rx=3 fill-opacity=0.2></rect><text text-anchor=middle dy=3 fill=white fill-opacity=0.6 class="text-[7px] font-black"></text></g><text y=-40 text-anchor=middle fill=white fill-opacity=0.4 class="text-[7px] font-bold uppercase tracking-widest"></svg>`,!1,!0,!1),hf=K(`<svg><g><circle r=12 fill=black fill-opacity=0.4 stroke=white stroke-opacity=0.1></circle><text text-anchor=middle dy=3 fill=white fill-opacity=0.3 class="text-[6px] font-black uppercase"></svg>`,!1,!0,!1);function gf(){let e=yn.meshTopology,t=yn.pulse,n=yn.meshPositions,r=yn.schemas,i=new Map;te(()=>{}),P(()=>{e().peers.forEach(e=>{let t=i.get(e.id);t&&!t._draggable&&(t._draggable=!0,(0,It.default)(t).draggable({listeners:{move(t){let r=n()[e.id]||{x:e.x||0,y:e.y||0};yn.setMeshPositions(n=>({...n,[e.id]:{x:r.x+t.dx,y:r.y+t.dy}}))}}}))})});let a=e=>e.includes(`ops`)?`C++`:e.includes(`export`)||e.includes(`ui`)?`JS`:`MESH`,o=F(()=>{let t=e().peers||[],r=window.innerWidth/2,i=window.innerHeight/2;return t.map((e,a)=>{let o=n()[e.id];if(o)return{...e,...o};let s=a/t.length*Math.PI*2;return{...e,x:r+Math.cos(s)*200-(t.length>1?100:0),y:i+Math.sin(s)*200}})}),s=e=>{let t=r();return Object.entries(t).filter(([t,n])=>n._origin===e).map(([e])=>e.split(`/`).pop())},c=F(()=>{let e=o(),t=[],n=new Set;return e.forEach(r=>{(r.neighbors||[]).forEach(i=>{let a=e.find(e=>e.id===i.id);if(a){let e=[r.id,a.id].sort().join(`--`);n.has(e)||(n.add(e),t.push({from:r,to:a,reachability:i.reachability}))}})}),t});return(()=>{var e=uf(),n=e.firstChild;return J(n,G(Ae,{get each(){return c()},children:e=>(()=>{var t=df();return N(n=>{var r=e.from.x,i=e.from.y,a=e.to.x,o=e.to.y,s=e.reachability===`REVERSE`?`4 4`:`0`;return r!==n.e&&Ge(t,`x1`,n.e=r),i!==n.t&&Ge(t,`y1`,n.t=i),a!==n.a&&Ge(t,`x2`,n.a=a),o!==n.o&&Ge(t,`y2`,n.o=o),s!==n.i&&Ge(t,`stroke-dasharray`,n.i=s),n},{e:void 0,t:void 0,a:void 0,o:void 0,i:void 0}),t})()}),null),J(n,G(Ae,{get each(){return t()},children:e=>{let t=e.payload?.stack||[];if(t.length<2)return null;let n=t[t.length-1],r=t[t.length-2],i=o().find(e=>e.id===n),a=o().find(e=>e.id===r);return!i||!a?null:(()=>{var e=ff(),t=e.firstChild;return N(()=>Ge(t,`path`,`M ${i.x} ${i.y} L ${a.x} ${a.y}`)),e})()}}),null),J(n,G(Ae,{get each(){return o()},children:e=>(()=>{var t=mf(),n=t.firstChild,r=n.nextSibling,o=r.nextSibling,c=o.firstChild,l=o.nextSibling,u=l.firstChild,d=u.nextSibling,f=l.nextSibling;return $e(t=>i.set(e.id,t),t),J(t,G(Ae,{get each(){return s(e.id)},children:(t,n)=>{let r=n/s(e.id).length*Math.PI*2;return(()=>{var e=hf(),n=e.firstChild.nextSibling;return J(n,()=>t.slice(0,3)),N(()=>Ge(e,`transform`,`translate(${Math.cos(r)*55}, ${Math.sin(r)*55})`)),e})()}}),n),J(c,(()=>{var t=Be(()=>e.type===`BROWSER`);return()=>t()?G(Bd,{size:16}):G(Pd,{size:16})})()),J(d,()=>a(e.id)),J(t,G(je,{get when(){return e.pps>0},get children(){var t=pf(),n=t.firstChild;return J(t,()=>e.pps,n),t}}),f),J(f,()=>e.id.split(`-`).pop()),N(i=>{var a=`translate(${e.x}, ${e.y})`,o=e.type===`BROWSER`?`#3b82f6`:`#f97316`,s=Math.min(.4,(e.pps||0)/10),c=e.type===`BROWSER`?`#3b82f6`:`#f97316`,l=e.type===`BROWSER`?`#3b82f6`:`#f97316`;return a!==i.e&&Ge(t,`transform`,i.e=a),o!==i.t&&Ge(n,`fill`,i.t=o),s!==i.a&&Ge(n,`fill-opacity`,i.a=s),c!==i.o&&Ge(r,`stroke`,i.o=c),l!==i.i&&Ge(u,`fill`,i.i=l),i},{e:void 0,t:void 0,a:void 0,o:void 0,i:void 0}),t})()}),null),e})()}var _f={type:`change`},vf={type:`start`},yf={type:`end`},bf=new Mi,xf=new Ao,Sf=Math.cos(70*jr.DEG2RAD),Cf=class extends ir{constructor(e,t){super(),this.object=e,this.domElement=t,this.domElement.style.touchAction=`none`,this.enabled=!0,this.target=new Z,this.cursor=new Z,this.minDistance=0,this.maxDistance=1/0,this.minZoom=0,this.maxZoom=1/0,this.minTargetRadius=0,this.maxTargetRadius=1/0,this.minPolarAngle=0,this.maxPolarAngle=Math.PI,this.minAzimuthAngle=-1/0,this.maxAzimuthAngle=1/0,this.enableDamping=!1,this.dampingFactor=.05,this.enableZoom=!0,this.zoomSpeed=1,this.enableRotate=!0,this.rotateSpeed=1,this.enablePan=!0,this.panSpeed=1,this.screenSpacePanning=!0,this.keyPanSpeed=7,this.zoomToCursor=!1,this.autoRotate=!1,this.autoRotateSpeed=2,this.keys={LEFT:`ArrowLeft`,UP:`ArrowUp`,RIGHT:`ArrowRight`,BOTTOM:`ArrowDown`},this.mouseButtons={LEFT:bn.ROTATE,MIDDLE:bn.DOLLY,RIGHT:bn.PAN},this.touches={ONE:xn.ROTATE,TWO:xn.DOLLY_PAN},this.target0=this.target.clone(),this.position0=this.object.position.clone(),this.zoom0=this.object.zoom,this._domElementKeyEvents=null,this.getPolarAngle=function(){return o.phi},this.getAzimuthalAngle=function(){return o.theta},this.getDistance=function(){return this.object.position.distanceTo(this.target)},this.listenToKeyEvents=function(e){e.addEventListener(`keydown`,ve),this._domElementKeyEvents=e},this.stopListenToKeyEvents=function(){this._domElementKeyEvents.removeEventListener(`keydown`,ve),this._domElementKeyEvents=null},this.saveState=function(){n.target0.copy(n.target),n.position0.copy(n.object.position),n.zoom0=n.object.zoom},this.reset=function(){n.target.copy(n.target0),n.object.position.copy(n.position0),n.object.zoom=n.zoom0,n.object.updateProjectionMatrix(),n.dispatchEvent(_f),n.update(),i=r.NONE},this.update=function(){let t=new Z,u=new ri().setFromUnitVectors(e.up,new Z(0,1,0)),d=u.clone().invert(),f=new Z,p=new ri,m=new Z,h=2*Math.PI;return function(g=null){let _=n.object.position;t.copy(_).sub(n.target),t.applyQuaternion(u),o.setFromVector3(t),n.autoRotate&&i===r.NONE&&D(T(g)),n.enableDamping?(o.theta+=s.theta*n.dampingFactor,o.phi+=s.phi*n.dampingFactor):(o.theta+=s.theta,o.phi+=s.phi);let v=n.minAzimuthAngle,S=n.maxAzimuthAngle;isFinite(v)&&isFinite(S)&&(v<-Math.PI?v+=h:v>Math.PI&&(v-=h),S<-Math.PI?S+=h:S>Math.PI&&(S-=h),v<=S?o.theta=Math.max(v,Math.min(S,o.theta)):o.theta=o.theta>(v+S)/2?Math.max(v,o.theta):Math.min(S,o.theta)),o.phi=Math.max(n.minPolarAngle,Math.min(n.maxPolarAngle,o.phi)),o.makeSafe(),n.enableDamping===!0?n.target.addScaledVector(l,n.dampingFactor):n.target.add(l),n.target.sub(n.cursor),n.target.clampLength(n.minTargetRadius,n.maxTargetRadius),n.target.add(n.cursor);let C=!1;if(n.zoomToCursor&&x||n.object.isOrthographicCamera)o.radius=F(o.radius);else{let e=o.radius;o.radius=F(o.radius*c),C=e!=o.radius}if(t.setFromSpherical(o),t.applyQuaternion(d),_.copy(n.target).add(t),n.object.lookAt(n.target),n.enableDamping===!0?(s.theta*=1-n.dampingFactor,s.phi*=1-n.dampingFactor,l.multiplyScalar(1-n.dampingFactor)):(s.set(0,0,0),l.set(0,0,0)),n.zoomToCursor&&x){let r=null;if(n.object.isPerspectiveCamera){let e=t.length();r=F(e*c);let i=e-r;n.object.position.addScaledVector(y,i),n.object.updateMatrixWorld(),C=!!i}else if(n.object.isOrthographicCamera){let e=new Z(b.x,b.y,0);e.unproject(n.object);let i=n.object.zoom;n.object.zoom=Math.max(n.minZoom,Math.min(n.maxZoom,n.object.zoom/c)),n.object.updateProjectionMatrix(),C=i!==n.object.zoom;let a=new Z(b.x,b.y,0);a.unproject(n.object),n.object.position.sub(a).add(e),n.object.updateMatrixWorld(),r=t.length()}else console.warn(`WARNING: OrbitControls.js encountered an unknown camera type - zoom to cursor disabled.`),n.zoomToCursor=!1;r!==null&&(this.screenSpacePanning?n.target.set(0,0,-1).transformDirection(n.object.matrix).multiplyScalar(r).add(n.object.position):(bf.origin.copy(n.object.position),bf.direction.set(0,0,-1).transformDirection(n.object.matrix),Math.abs(n.object.up.dot(bf.direction))<Sf?e.lookAt(n.target):(xf.setFromNormalAndCoplanarPoint(n.object.up,n.target),bf.intersectPlane(xf,n.target))))}else if(n.object.isOrthographicCamera){let e=n.object.zoom;n.object.zoom=Math.max(n.minZoom,Math.min(n.maxZoom,n.object.zoom/c)),e!==n.object.zoom&&(n.object.updateProjectionMatrix(),C=!0)}return c=1,x=!1,C||f.distanceToSquared(n.object.position)>a||8*(1-p.dot(n.object.quaternion))>a||m.distanceToSquared(n.target)>a?(n.dispatchEvent(_f),f.copy(n.object.position),p.copy(n.object.quaternion),m.copy(n.target),!0):!1}}(),this.dispose=function(){n.domElement.removeEventListener(`contextmenu`,xe),n.domElement.removeEventListener(`pointerdown`,W),n.domElement.removeEventListener(`pointercancel`,de),n.domElement.removeEventListener(`wheel`,me),n.domElement.removeEventListener(`pointermove`,ue),n.domElement.removeEventListener(`pointerup`,de),n.domElement.getRootNode().removeEventListener(`keydown`,ge,{capture:!0}),n._domElementKeyEvents!==null&&(n._domElementKeyEvents.removeEventListener(`keydown`,ve),n._domElementKeyEvents=null)};let n=this,r={NONE:-1,ROTATE:0,DOLLY:1,PAN:2,TOUCH_ROTATE:3,TOUCH_PAN:4,TOUCH_DOLLY_PAN:5,TOUCH_DOLLY_ROTATE:6},i=r.NONE,a=1e-6,o=new Du,s=new Du,c=1,l=new Z,u=new Y,d=new Y,f=new Y,p=new Y,m=new Y,h=new Y,g=new Y,_=new Y,v=new Y,y=new Z,b=new Y,x=!1,S=[],C={},w=!1;function T(e){return e===null?2*Math.PI/60/60*n.autoRotateSpeed:2*Math.PI/60*n.autoRotateSpeed*e}function E(e){let t=Math.abs(e*.01);return .95**(n.zoomSpeed*t)}function D(e){s.theta-=e}function O(e){s.phi-=e}let k=function(){let e=new Z;return function(t,n){e.setFromMatrixColumn(n,0),e.multiplyScalar(-t),l.add(e)}}(),A=function(){let e=new Z;return function(t,r){n.screenSpacePanning===!0?e.setFromMatrixColumn(r,1):(e.setFromMatrixColumn(r,0),e.crossVectors(n.object.up,e)),e.multiplyScalar(t),l.add(e)}}(),j=function(){let e=new Z;return function(t,r){let i=n.domElement;if(n.object.isPerspectiveCamera){let a=n.object.position;e.copy(a).sub(n.target);let o=e.length();o*=Math.tan(n.object.fov/2*Math.PI/180),k(2*t*o/i.clientHeight,n.object.matrix),A(2*r*o/i.clientHeight,n.object.matrix)}else n.object.isOrthographicCamera?(k(t*(n.object.right-n.object.left)/n.object.zoom/i.clientWidth,n.object.matrix),A(r*(n.object.top-n.object.bottom)/n.object.zoom/i.clientHeight,n.object.matrix)):(console.warn(`WARNING: OrbitControls.js encountered an unknown camera type - pan disabled.`),n.enablePan=!1)}}();function M(e){n.object.isPerspectiveCamera||n.object.isOrthographicCamera?c/=e:(console.warn(`WARNING: OrbitControls.js encountered an unknown camera type - dolly/zoom disabled.`),n.enableZoom=!1)}function N(e){n.object.isPerspectiveCamera||n.object.isOrthographicCamera?c*=e:(console.warn(`WARNING: OrbitControls.js encountered an unknown camera type - dolly/zoom disabled.`),n.enableZoom=!1)}function P(e,t){if(!n.zoomToCursor)return;x=!0;let r=n.domElement.getBoundingClientRect(),i=e-r.left,a=t-r.top,o=r.width,s=r.height;b.x=i/o*2-1,b.y=-(a/s)*2+1,y.set(b.x,b.y,1).unproject(n.object).sub(n.object.position).normalize()}function F(e){return Math.max(n.minDistance,Math.min(n.maxDistance,e))}function ee(e){u.set(e.clientX,e.clientY)}function I(e){P(e.clientX,e.clientX),g.set(e.clientX,e.clientY)}function te(e){p.set(e.clientX,e.clientY)}function ne(e){d.set(e.clientX,e.clientY),f.subVectors(d,u).multiplyScalar(n.rotateSpeed);let t=n.domElement;D(2*Math.PI*f.x/t.clientHeight),O(2*Math.PI*f.y/t.clientHeight),u.copy(d),n.update()}function re(e){_.set(e.clientX,e.clientY),v.subVectors(_,g),v.y>0?M(E(v.y)):v.y<0&&N(E(v.y)),g.copy(_),n.update()}function ie(e){m.set(e.clientX,e.clientY),h.subVectors(m,p).multiplyScalar(n.panSpeed),j(h.x,h.y),p.copy(m),n.update()}function ae(e){P(e.clientX,e.clientY),e.deltaY<0?N(E(e.deltaY)):e.deltaY>0&&M(E(e.deltaY)),n.update()}function oe(e){let t=!1;switch(e.code){case n.keys.UP:e.ctrlKey||e.metaKey||e.shiftKey?O(2*Math.PI*n.rotateSpeed/n.domElement.clientHeight):j(0,n.keyPanSpeed),t=!0;break;case n.keys.BOTTOM:e.ctrlKey||e.metaKey||e.shiftKey?O(-2*Math.PI*n.rotateSpeed/n.domElement.clientHeight):j(0,-n.keyPanSpeed),t=!0;break;case n.keys.LEFT:e.ctrlKey||e.metaKey||e.shiftKey?D(2*Math.PI*n.rotateSpeed/n.domElement.clientHeight):j(n.keyPanSpeed,0),t=!0;break;case n.keys.RIGHT:e.ctrlKey||e.metaKey||e.shiftKey?D(-2*Math.PI*n.rotateSpeed/n.domElement.clientHeight):j(-n.keyPanSpeed,0),t=!0;break}t&&(e.preventDefault(),n.update())}function L(e){if(S.length===1)u.set(e.pageX,e.pageY);else{let t=Te(e),n=.5*(e.pageX+t.x),r=.5*(e.pageY+t.y);u.set(n,r)}}function se(e){if(S.length===1)p.set(e.pageX,e.pageY);else{let t=Te(e),n=.5*(e.pageX+t.x),r=.5*(e.pageY+t.y);p.set(n,r)}}function ce(e){let t=Te(e),n=e.pageX-t.x,r=e.pageY-t.y,i=Math.sqrt(n*n+r*r);g.set(0,i)}function R(e){n.enableZoom&&ce(e),n.enablePan&&se(e)}function le(e){n.enableZoom&&ce(e),n.enableRotate&&L(e)}function z(e){if(S.length==1)d.set(e.pageX,e.pageY);else{let t=Te(e),n=.5*(e.pageX+t.x),r=.5*(e.pageY+t.y);d.set(n,r)}f.subVectors(d,u).multiplyScalar(n.rotateSpeed);let t=n.domElement;D(2*Math.PI*f.x/t.clientHeight),O(2*Math.PI*f.y/t.clientHeight),u.copy(d)}function B(e){if(S.length===1)m.set(e.pageX,e.pageY);else{let t=Te(e),n=.5*(e.pageX+t.x),r=.5*(e.pageY+t.y);m.set(n,r)}h.subVectors(m,p).multiplyScalar(n.panSpeed),j(h.x,h.y),p.copy(m)}function V(e){let t=Te(e),r=e.pageX-t.x,i=e.pageY-t.y,a=Math.sqrt(r*r+i*i);_.set(0,a),v.set(0,(_.y/g.y)**+n.zoomSpeed),M(v.y),g.copy(_),P((e.pageX+t.x)*.5,(e.pageY+t.y)*.5)}function H(e){n.enableZoom&&V(e),n.enablePan&&B(e)}function U(e){n.enableZoom&&V(e),n.enableRotate&&z(e)}function W(e){n.enabled!==!1&&(S.length===0&&(n.domElement.setPointerCapture(e.pointerId),n.domElement.addEventListener(`pointermove`,ue),n.domElement.addEventListener(`pointerup`,de)),!Ce(e)&&(Se(e),e.pointerType===`touch`?ye(e):fe(e)))}function ue(e){n.enabled!==!1&&(e.pointerType===`touch`?be(e):pe(e))}function de(e){switch(G(e),S.length){case 0:n.domElement.releasePointerCapture(e.pointerId),n.domElement.removeEventListener(`pointermove`,ue),n.domElement.removeEventListener(`pointerup`,de),n.dispatchEvent(yf),i=r.NONE;break;case 1:let t=S[0],a=C[t];ye({pointerId:t,pageX:a.x,pageY:a.y});break}}function fe(e){let t;switch(e.button){case 0:t=n.mouseButtons.LEFT;break;case 1:t=n.mouseButtons.MIDDLE;break;case 2:t=n.mouseButtons.RIGHT;break;default:t=-1}switch(t){case bn.DOLLY:if(n.enableZoom===!1)return;I(e),i=r.DOLLY;break;case bn.ROTATE:if(e.ctrlKey||e.metaKey||e.shiftKey){if(n.enablePan===!1)return;te(e),i=r.PAN}else{if(n.enableRotate===!1)return;ee(e),i=r.ROTATE}break;case bn.PAN:if(e.ctrlKey||e.metaKey||e.shiftKey){if(n.enableRotate===!1)return;ee(e),i=r.ROTATE}else{if(n.enablePan===!1)return;te(e),i=r.PAN}break;default:i=r.NONE}i!==r.NONE&&n.dispatchEvent(vf)}function pe(e){switch(i){case r.ROTATE:if(n.enableRotate===!1)return;ne(e);break;case r.DOLLY:if(n.enableZoom===!1)return;re(e);break;case r.PAN:if(n.enablePan===!1)return;ie(e);break}}function me(e){n.enabled===!1||n.enableZoom===!1||i!==r.NONE||(e.preventDefault(),n.dispatchEvent(vf),ae(he(e)),n.dispatchEvent(yf))}function he(e){let t=e.deltaMode,n={clientX:e.clientX,clientY:e.clientY,deltaY:e.deltaY};switch(t){case 1:n.deltaY*=16;break;case 2:n.deltaY*=100;break}return e.ctrlKey&&!w&&(n.deltaY*=10),n}function ge(e){e.key===`Control`&&(w=!0,n.domElement.getRootNode().addEventListener(`keyup`,_e,{passive:!0,capture:!0}))}function _e(e){e.key===`Control`&&(w=!1,n.domElement.getRootNode().removeEventListener(`keyup`,_e,{passive:!0,capture:!0}))}function ve(e){n.enabled===!1||n.enablePan===!1||oe(e)}function ye(e){switch(we(e),S.length){case 1:switch(n.touches.ONE){case xn.ROTATE:if(n.enableRotate===!1)return;L(e),i=r.TOUCH_ROTATE;break;case xn.PAN:if(n.enablePan===!1)return;se(e),i=r.TOUCH_PAN;break;default:i=r.NONE}break;case 2:switch(n.touches.TWO){case xn.DOLLY_PAN:if(n.enableZoom===!1&&n.enablePan===!1)return;R(e),i=r.TOUCH_DOLLY_PAN;break;case xn.DOLLY_ROTATE:if(n.enableZoom===!1&&n.enableRotate===!1)return;le(e),i=r.TOUCH_DOLLY_ROTATE;break;default:i=r.NONE}break;default:i=r.NONE}i!==r.NONE&&n.dispatchEvent(vf)}function be(e){switch(we(e),i){case r.TOUCH_ROTATE:if(n.enableRotate===!1)return;z(e),n.update();break;case r.TOUCH_PAN:if(n.enablePan===!1)return;B(e),n.update();break;case r.TOUCH_DOLLY_PAN:if(n.enableZoom===!1&&n.enablePan===!1)return;H(e),n.update();break;case r.TOUCH_DOLLY_ROTATE:if(n.enableZoom===!1&&n.enableRotate===!1)return;U(e),n.update();break;default:i=r.NONE}}function xe(e){n.enabled!==!1&&e.preventDefault()}function Se(e){S.push(e.pointerId)}function G(e){delete C[e.pointerId];for(let t=0;t<S.length;t++)if(S[t]==e.pointerId){S.splice(t,1);return}}function Ce(e){for(let t=0;t<S.length;t++)if(S[t]==e.pointerId)return!0;return!1}function we(e){let t=C[e.pointerId];t===void 0&&(t=new Y,C[e.pointerId]=t),t.set(e.pageX,e.pageY)}function Te(e){return C[e.pointerId===S[0]?S[1]:S[0]]}n.domElement.addEventListener(`contextmenu`,xe),n.domElement.addEventListener(`pointerdown`,W),n.domElement.addEventListener(`pointercancel`,de),n.domElement.addEventListener(`wheel`,me,{passive:!1}),n.domElement.getRootNode().addEventListener(`keydown`,ge,{passive:!0,capture:!0}),this.update()}},wf=K(`<div class="w-full h-full rounded-lg overflow-hidden">`),Tf=e=>{let t,n,r,i,a,o,s=()=>{if(!t)return;let e=t.clientWidth,s=t.clientHeight;r=new Ul,r.background=new Sa(1976635),i=new xo(45,e/s,.1,1e4),i.position.set(0,0,1),i.up.set(0,1,0),i.lookAt(0,0,0),n=new Vl({antialias:!0}),n.setSize(e,s),n.setPixelRatio(window.devicePixelRatio),t.appendChild(n.domElement),a=new Cf(i,n.domElement),a.enableDamping=!0,r.add(new mu(16777215,.6));let c=new pu(16777215,.8);c.position.set(100,200,100),r.add(c);let l=()=>{o=requestAnimationFrame(l),a.update(),n.render(r,i)};l();let u=new ResizeObserver(()=>{let e=t.clientWidth,r=t.clientHeight;n.setSize(e,r),i.aspect=e/r,i.updateProjectionMatrix()});u.observe(t),ne(()=>{cancelAnimationFrame(o),u.disconnect(),n.dispose(),t&&n.domElement&&t.removeChild(n.domElement)})};return te(()=>{s()}),P(async()=>{let t=e.data;if(t&&r){for(;r.children.length>0;){let e=r.children[0];e.type===`Mesh`&&(e.geometry.dispose(),e.material.dispose()),r.remove(e)}r.add(new mu(16777215,.6));let e=new pu(16777215,.8);e.position.set(100,200,100),r.add(e);try{await gd(tn,t,r);let e=new oi().setFromObject(r),n=e.getCenter(new Z),o=e.getSize(new Z),s=(Math.max(o.x,o.y,o.z)||1)*2;i.position.set(n.x,n.y,n.z+s),i.lookAt(n),i.up.set(0,1,0),a.target.copy(n),a.update()}catch(e){console.warn(`[Viewport] Failed to render data:`,e)}}}),(()=>{var e=wf();e.$$contextmenu=e=>e.stopPropagation(),e.$$dblclick=e=>e.stopPropagation(),e.$$pointerdown=e=>e.stopPropagation(),e.addEventListener(`wheel`,e=>e.stopPropagation()),e.$$mouseup=e=>e.stopPropagation(),e.$$mousemove=e=>e.stopPropagation(),e.$$mousedown=e=>e.stopPropagation();var n=t;return typeof n==`function`?$e(n,e):t=e,e})()};We([`mousedown`,`mousemove`,`mouseup`,`pointerdown`,`dblclick`,`contextmenu`]);var Ef=`modulepreload`,Df=function(e){return`/`+e},Of={},kf=function(e,t,n){let r=Promise.resolve();if(t&&t.length>0){let e=document.getElementsByTagName(`link`),i=document.querySelector(`meta[property=csp-nonce]`),a=i?.nonce||i?.getAttribute(`nonce`);function o(e){return Promise.all(e.map(e=>Promise.resolve(e).then(e=>({status:`fulfilled`,value:e}),e=>({status:`rejected`,reason:e}))))}r=o(t.map(t=>{if(t=Df(t,n),t in Of)return;Of[t]=!0;let r=t.endsWith(`.css`),i=r?`[rel="stylesheet"]`:``;if(n)for(let n=e.length-1;n>=0;n--){let i=e[n];if(i.href===t&&(!r||i.rel===`stylesheet`))return}else if(document.querySelector(`link[href="${t}"]${i}`))return;let o=document.createElement(`link`);if(o.rel=r?`stylesheet`:Ef,r||(o.as=`script`),o.crossOrigin=``,o.href=t,a&&o.setAttribute(`nonce`,a),document.head.appendChild(o),r)return new Promise((e,n)=>{o.addEventListener(`load`,e),o.addEventListener(`error`,()=>n(Error(`Unable to preload CSS for ${t}`)))})}))}function i(e){let t=new Event(`vite:preloadError`,{cancelable:!0});if(t.payload=e,window.dispatchEvent(t),!t.defaultPrevented)throw e}return r.then(t=>{for(let e of t||[])e.status===`rejected`&&i(e.reason);return e().catch(i)})},Af=K(`<div class="w-full mt-3 border-t border-white/10 pt-3"><div class="flex items-center justify-between mb-2"><div class="text-[9px] font-black tracking-widest text-capability/80 uppercase flex items-center gap-1"><span>Agent Custom UI</span></div><div>`),jf=K(`<div class="text-red-400 text-xs p-2 bg-red-900/20 rounded border border-red-500/20">Failed to load UI: `),Mf=K(`<div class="min-h-12 flex items-center justify-center text-white/30 text-[10px] italic">`),Nf=e=>{let t,[n,r]=M(null),[i,a]=M(!1),[o,s]=M(`initializing`);return P(async()=>{let t=e.schema;if(!t||!t.ux||!t.componentName){s(`no-ux-config`);return}try{s(`fetching-script`);let e=t.ux.replace(`vfs:/`,``);console.log(`[DynamicUX] Fetching UI script from: ${e}`);let n=await tn.readData(e,{});if(!n||typeof n!=`string`)throw console.error(`[DynamicUX] Script content is empty or invalid:`,n),Error(`Empty script at ${e}`);console.log(`[DynamicUX] Received script (${n.length} chars). First 50: "${n.slice(0,50)}..."`),s(`registering-component`);let r=new Blob([n],{type:`text/javascript`}),i=URL.createObjectURL(r);await kf(()=>import(i),[]),URL.revokeObjectURL(i),console.log(`[DynamicUX] Component registered: ${t.componentName}`),a(!0),s(`ready`)}catch(e){console.error(`[DynamicUX] Error:`,e),r(e.message),s(`error`)}}),P(()=>{if(i()&&t&&e.schema.componentName){t.innerHTML=``;let n=document.createElement(e.schema.componentName);if(e.parameters)for(let[t,r]of Object.entries(e.parameters))n.setAttribute(t,r);let r=t=>{t.detail&&yn.tickle(e.path,t.detail)};n.addEventListener(`param-change`,r),t.appendChild(n),ne(()=>n.removeEventListener(`param-change`,r))}}),(()=>{var e=Af(),r=e.firstChild.firstChild.nextSibling;return J(r,()=>o().toUpperCase()),J(e,(()=>{var e=Be(()=>!!n());return()=>e()?(()=>{var e=jf();return e.firstChild,J(e,n,null),e})():(()=>{var e=Mf(),n=t;return typeof n==`function`?$e(n,e):t=e,J(e,()=>i()?``:`Loading agent UI...`),e})()})(),null),N(()=>Je(r,`text-[7px] font-mono px-1.5 py-0.5 rounded border ${o()===`ready`?`bg-green-500/20 border-green-500/40 text-green-400`:o()===`error`?`bg-red-500/20 border-red-500/40 text-red-400`:`bg-white/5 border-white/10 text-white/40`}`)),e})()},Pf=K(`<span class="text-blackboard text-[8px] font-black">`),Ff=K(`<div>`),If=K(`<img class="w-full h-full object-cover rounded-lg p-0.5">`),Lf=K(`<div class="w-16 h-16 rounded-xl border-2 border-white/10 shadow-2xl flex items-center justify-center bg-black/40 backdrop-blur-md relative group hover:border-white/30 transition-all overflow-visible">`),Rf=K(`<div class="px-2 py-0.5 rounded-full text-[8px] font-black tracking-widest whitespace-nowrap bg-blackboard/80 border border-white/5 text-white/60 shadow-lg uppercase">`),zf=K(`<div class="col-span-2 py-2 text-center opacity-30 italic">Loading schema properties...`),Bf=K(`<div class="bg-slate-500/10 border border-slate-500/20 rounded-lg p-2 flex flex-col gap-1.5 mb-1"><div class="flex items-center gap-1.5 text-slate-400"><span class="text-[9px] font-black tracking-widest uppercase">Schema Defined</span></div><div class="text-[8px] font-mono opacity-60 grid grid-cols-2 gap-x-2 gap-y-1">`),Vf=K(`<div class="py-8 flex flex-col items-center gap-2 opacity-20 italic"><span class=text-[10px]>No data at this path yet.`),Hf=K(`<div class="w-80 h-auto rounded-xl p-4 flex flex-col text-white bg-blackboard border-2 border-white/10 shadow-2xl"><div class="flex items-center justify-between border-b border-white/10 pb-2"><div class="flex items-center gap-2 relative"><span class="text-xs font-black tracking-widest truncate"></span></div><button class="text-[10px] font-bold opacity-40 hover:opacity-100 hover:text-red-400 transition-colors">CLOSE</button></div><div class="w-full h-64 bg-black/40 rounded-lg border border-white/5 overflow-hidden relative group"><div class="absolute bottom-2 right-2 px-1.5 py-0.5 bg-black/60 rounded text-[8px] font-mono text-white/40 pointer-events-none group-hover:text-white/80 transition-colors">INTERACTIVE VIEW</div></div><div class="flex flex-col gap-2 max-h-96 overflow-y-auto pr-1 custom-scrollbar">`),Uf=K(`<div style=left:0px;top:0px;pointer-events:auto>`),Wf=K(`<img class="w-full h-full object-contain opacity-50">`),Gf=K(`<span class=text-white/40>:`),Kf=K(`<span class="truncate text-white/80">`),qf=K(`<span class="text-[7px] px-1 bg-white/5 rounded text-white/30">`),Jf=K(`<div class="bg-black/40 rounded-lg p-2 border border-white/5 flex flex-col gap-1.5 hover:border-white/20 transition-colors"><div class="flex justify-between items-center"><span></span><span class="text-[7px] opacity-20 font-mono"></span></div><div class="text-[9px] font-mono text-white/70 break-all bg-black/20 p-1.5 rounded"></div><div class="flex justify-between items-center mt-1"><div class="flex items-center gap-1"></div><button class="px-2 py-0.5 bg-white/5 hover:bg-available hover:text-black rounded text-[8px] font-bold transition-all">REFRESH`),Yf=K(`<div class="text-[5px] opacity-40 font-mono tracking-tighter text-white uppercase">`),Xf=K(`<div class="absolute w-14 h-14 flex items-center justify-center cursor-move z-20 group"style=left:0px;top:0px;pointer-events:auto><svg viewBox="0 0 100 100"class="absolute inset-0 w-full h-full drop-shadow-2xl"><polygon points="28,8 72,8 94,50 72,92 28,92 6,50"fill=#f97316 stroke=rgba(255,255,255,0.2) stroke-width=4 class="group-hover:fill-orange-400 transition-colors"></polygon></svg><div class="absolute top-full mt-2 flex flex-col items-center gap-0.5 z-10 pointer-events-none"><div class="px-1.5 py-0.5 rounded text-[7px] font-black bg-black/80 backdrop-blur-sm text-capability whitespace-nowrap border border-capability/20 shadow-lg">`),Zf=K(`<svg><line stroke-width=2></svg>`,!1,!0,!1),Qf=K(`<div class="canvas-container w-full h-full overflow-hidden bg-blackboard relative"><div class="absolute top-4 left-4 z-50 flex items-center gap-2 px-3 py-1.5 rounded-full bg-black/80 backdrop-blur-md border border-white/20 shadow-xl"><div></div><span class="text-[10px] font-black text-white/70 tracking-widest uppercase"></span></div><svg class="absolute inset-0 w-full h-full pointer-events-none z-0"></svg><div class="absolute inset-0 z-10 pointer-events-none overflow-hidden"><div class=pointer-events-auto></div></div><div class="absolute bottom-6 left-6 flex flex-col gap-1 pointer-events-none"><div class="text-[10px] font-black text-white/20 tracking-[0.4em]">JotCAD Distributed Blackboard</div><div class="text-[8px] font-mono text-white/10 tracking-widest opacity-50 uppercase">Double-click Path [Circle] to view results • Orange Hexagons = Agents`),$f=e=>{let t=[];e.states.includes(`SCHEMA`)&&t.push({id:`schema`,color:`bg-slate-600`,title:`Schema Declared`,icon:G(Rd,{size:10,class:`text-white`})}),e.states.includes(`AVAILABLE`)?t.push({id:`available`,color:`bg-white`,borderColor:`border-node`,title:`Available`,icon:G(Od,{size:10,class:`text-node stroke-[4]`})}):e.states.includes(`PROVISIONING`)?t.push({id:`provisioning`,color:`bg-provisioning`,title:`Provisioning`,animate:`animate-pulse`}):e.states.includes(`PENDING`)&&t.push({id:`pending`,color:`bg-pending`,title:`Pending`,animate:`animate-bounce`}),e.hasThumbnail?t.push({id:`thumb-ready`,color:`bg-blue-600`,title:`Thumbnail Ready`,icon:G(Ad,{size:10,class:`text-white`})}):e.isGenerating&&t.push({id:`thumb-gen`,color:`bg-cyan-500`,title:`Generating Thumbnail...`,animate:`animate-pulse`,icon:G(wd,{size:10,class:`text-white`})}),e.resultCount>1&&t.push({id:`count`,color:`bg-white`,title:`${e.resultCount} results`,content:(()=>{var t=Pf();return J(t,()=>e.resultCount),t})()});let n=[{top:`-6px`,left:`-6px`},{top:`-6px`,left:`14px`},{top:`-6px`,left:`34px`},{top:`-6px`,left:`54px`},{top:`14px`,left:`54px`},{top:`34px`,left:`54px`},{top:`54px`,left:`54px`},{top:`54px`,left:`34px`},{top:`54px`,left:`14px`},{top:`54px`,left:`-6px`}];return G(Ae,{each:t,children:(e,t)=>(()=>{var r=Ff();return J(r,()=>e.icon||e.content),N(i=>{var a=`absolute w-4 h-4 rounded-full border border-blackboard flex items-center justify-center shadow-lg z-20 transition-all duration-300 ${e.color} ${e.borderColor||``} ${e.animate||``}`,o=n[t()]?.top||`0px`,s=n[t()]?.left||`0px`,c=e.title;return a!==i.e&&Je(r,i.e=a),o!==i.t&&Ze(r,`top`,i.t=o),s!==i.a&&Ze(r,`left`,i.a=s),c!==i.o&&Ge(r,`title`,i.o=c),i},{e:void 0,t:void 0,a:void 0,o:void 0}),r})()})},ep=e=>{let t,[n,r]=M(!1),[i,a]=M(null),[o,s]=M(!1),[c,l]=M(null);te(()=>{(0,It.default)(t).draggable({listeners:{move(t){e.onMove(e.path,t.dx,t.dy)}}})}),P(async()=>{if(n()&&!c()){let t=e.results.find(e=>e.state===`AVAILABLE`);if(t)try{l(await tn.readData(t.path,t.parameters))}catch(e){console.warn(`[PathNode] Failed to fetch view data:`,e)}}}),P(async()=>{let t=e.results,n=t.find(e=>e.state===`AVAILABLE`),r=t.find(e=>e.state===`SCHEMA`),c=I(o),l=I(i),u=e.path.includes(`mesh`)||e.path.includes(`triangle`)||e.path.includes(`box`)||r&&r.data?.type===`mesh`;if(n&&!l&&!c&&u){s(!0);try{let e=await tn.readData(n.path,n.parameters);if(e){let t=null;typeof e==`string`&&(e.startsWith(`
=`)||e.includes(`v `))?t=await vd(tn,e):typeof e==`object`&&(e.geometry||e.shapes)&&(t=await vd(tn,`\n=${JSON.stringify(e).length} files/main.json\n${JSON.stringify(e)}`)),t&&a(t)}}catch(e){console.warn(`[Thumbnail] Failed to generate:`,e)}finally{s(!1)}}});let u=()=>{let t=e.path.toLowerCase(),r=n()?20:24;return t.includes(`box`)?G(Ed,{size:r,class:`opacity-20`}):t.includes(`triangle`)?G(Jd,{size:r,class:`opacity-20`}):t.includes(`mesh`)?G(Hd,{size:r,class:`opacity-20`}):G(Rd,{size:r,class:`opacity-20`})},d=()=>e.results.map(e=>e.state),f=F(()=>e.results.find(e=>e.state===`SCHEMA`));return(()=>{var a=Uf();a.$$dblclick=()=>r(!n());var s=t;return typeof s==`function`?$e(s,a):t=a,J(a,G(je,{get when(){return!n()},get children(){return[(()=>{var t=Lf();return J(t,G($f,{get states(){return d()},get hasThumbnail(){return!!i()},get isGenerating(){return o()},get resultCount(){return e.results.length}}),null),J(t,G(je,{get when(){return i()},get fallback(){return G(u,{})},get children(){var e=If();return N(()=>Ge(e,`src`,i())),e}}),null),t})(),(()=>{var t=Rf();return J(t,()=>e.path.split(`/`).pop()),t})()]}}),null),J(a,G(je,{get when(){return n()},get children(){var t=Hf(),n=t.firstChild,a=n.firstChild,s=a.firstChild,l=a.nextSibling,u=n.nextSibling,p=u.firstChild,m=u.nextSibling;return J(a,G($f,{get states(){return d()},get hasThumbnail(){return!!i()},get isGenerating(){return o()},get resultCount(){return e.results.length}}),s),J(a,G(Wd,{size:16,class:`opacity-50 text-available`}),s),J(s,()=>e.path),l.$$click=()=>r(!1),J(u,G(je,{get when(){return Be(()=>!!c())()&&(e.path.includes(`mesh`)||e.path.includes(`triangle`)||e.path.includes(`box`))},get fallback(){return G(je,{get when(){return i()},get children(){var e=Wf();return N(()=>Ge(e,`src`,i())),e}})},get children(){return G(Tf,{get data(){return c()}})}}),p),J(t,G(je,{get when(){return f()},get children(){var e=Bf(),t=e.firstChild,n=t.firstChild,r=t.nextSibling;return J(t,G(Rd,{size:12}),n),J(r,G(Ae,{get each(){return Object.entries(f().data?.properties||f().data||{})},children:([e,t])=>[(()=>{var t=Gf(),n=t.firstChild;return J(t,e,n),t})(),(()=>{var e=Kf();return J(e,()=>JSON.stringify(t)),e})()]}),null),J(r,G(je,{get when(){return!f().data},get children(){return zf()}}),null),e}}),m),J(t,G(je,{get when(){return f()?.data?.ux},get children(){return G(Nf,{get schema(){return f().data},get path(){return e.path},get parameters(){return e.results[0]?.parameters}})}}),m),J(m,G(Ae,{get each(){return e.results},children:t=>(()=>{var n=Jf(),r=n.firstChild,i=r.firstChild,a=i.nextSibling,o=r.nextSibling,s=o.nextSibling.firstChild,c=s.nextSibling;return J(i,()=>t.state),J(a,()=>t.cid.slice(0,8)),J(o,()=>JSON.stringify(t.parameters)),J(s,G(je,{get when(){return t.source},get children(){var e=qf();return J(e,()=>t.source),e}})),c.$$click=()=>yn.tickle(e.path,t.parameters),N(()=>Je(i,`text-[8px] font-black tracking-widest ${t.state===`AVAILABLE`?`text-available`:`text-white/40`}`)),n})()}),null),J(m,G(je,{get when(){return e.results.length===0},get children(){var e=Vf(),t=e.firstChild;return J(e,G(Md,{size:24}),t),e}}),null),t}}),null),N(t=>{var r=`absolute select-none cursor-move flex flex-col items-center gap-2 ${n()?`z-50`:`z-10`}`,i=`translate(${e.pos?.x||0}px, ${e.pos?.y||0}px)`;return r!==t.e&&Je(a,t.e=r),i!==t.t&&Ze(a,`transform`,t.t=i),t},{e:void 0,t:void 0}),a})()},tp=e=>{let t;return te(()=>{(0,It.default)(t).draggable({listeners:{move(t){e.onMove(e.data.cid,t.dx,t.dy)}}})}),(()=>{var n=Xf(),r=n.firstChild.nextSibling,i=r.firstChild,a=t;return typeof a==`function`?$e(a,n):t=n,J(n,G(Pd,{size:20,class:`text-blackboard relative z-10`}),r),J(i,()=>e.data.path),J(r,G(je,{get when(){return e.data.source},get children(){var t=Yf();return J(t,()=>e.data.source),t}}),null),N(t=>Ze(n,`transform`,`translate(${e.pos?.x||0}px, ${e.pos?.y||0}px)`)),n})()},np=e=>(()=>{var t=Zf();return N(n=>{var r=e.fromPos?.x+e.fromSize/2,i=e.fromPos?.y+e.fromSize/2,a=e.toPos?.x+e.toSize/2,o=e.toPos?.y+e.toSize/2,s=e.color,c=e.opacity,l=e.dashed?`4 4`:``;return r!==n.e&&Ge(t,`x1`,n.e=r),i!==n.t&&Ge(t,`y1`,n.t=i),a!==n.a&&Ge(t,`x2`,n.a=a),o!==n.o&&Ge(t,`y2`,n.o=o),s!==n.i&&Ge(t,`stroke`,n.i=s),c!==n.n&&Ge(t,`stroke-opacity`,n.n=c),l!==n.s&&Ge(t,`stroke-dasharray`,n.s=l),n},{e:void 0,t:void 0,a:void 0,o:void 0,i:void 0,n:void 0,s:void 0}),t})(),rp=()=>{let[e,t]=Nt({}),n;te(()=>{yn.start();let e=ld();n.appendChild(e.domElement);let t=()=>{ud(),requestAnimationFrame(t)};t()});let r=F(()=>{let e=yn.graph(),t={},n=[];return Object.values(e).forEach(e=>{e.state===`LISTENING`?(n.push(e),t[e.path]||(t[e.path]=[])):e.state===`SCHEMA`?(t[e.path.replace(/@schema$/,``)]||(t[e.path.replace(/@schema$/,``)]=[]),t[e.path.replace(/@schema$/,``)].push(e)):(t[e.path]||(t[e.path]=[]),t[e.path].push(e))}),{paths:t,agents:n}});P(()=>{let{paths:n,agents:i}=r();Object.keys(n).forEach((n,r)=>{e[n]||t(n,{x:350+r*180%(window.innerWidth-450),y:150+Math.floor(r/4)*180})}),i.forEach((n,r)=>{e[n.cid]||t(n.cid,{x:120,y:120+r*120})})});let i=(e,n,r)=>{t(e,`x`,e=>e+n),t(e,`y`,e=>e+r)},a=F(()=>{let{paths:t,agents:n}=r(),i=[];return n.forEach(n=>{let r=e[n.cid];r&&Object.keys(t).forEach(t=>{if(t.startsWith(n.path)){let n=e[t];n&&i.push({from:r,fromSize:56,to:n,toSize:48,color:`#f97316`,opacity:.4})}})}),i});return(()=>{var t=Qf(),o=t.firstChild,s=o.firstChild,c=s.nextSibling,l=o.nextSibling,u=l.nextSibling,d=u.firstChild,f=n;return typeof f==`function`?$e(f,t):n=t,J(t,G(gf,{}),o),J(c,()=>yn.isConnected()?`Mesh Online`:`Mesh Offline`),J(l,G(Ae,{get each(){return a()},children:e=>G(np,{get fromPos(){return e.from},get fromSize(){return e.fromSize},get toPos(){return e.to},get toSize(){return e.toSize},get color(){return e.color},get opacity(){return e.opacity}})})),J(d,G(Zd,{}),null),J(d,G(af,{}),null),J(d,G(lf,{}),null),J(u,G(Ae,{get each(){return Object.keys(r().paths)},children:t=>G(ep,{path:t,get results(){return r().paths[t]},get pos(){return e[t]},onMove:i})}),null),J(u,G(Ae,{get each(){return r().agents},children:t=>G(tp,{data:t,get pos(){return e[t.cid]},onMove:i})}),null),N(()=>Je(s,`w-2.5 h-2.5 rounded-full ${yn.isConnected()?`bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]`:`bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.6)]`}`)),t})()};We([`dblclick`,`click`]);var ip=K(`<div class="w-screen h-screen">`);function ap(){return(()=>{var e=ip();return J(e,G(rp,{})),e})()}var op=document.getElementById(`root`);op&&Ue(()=>G(ap,{}),op);
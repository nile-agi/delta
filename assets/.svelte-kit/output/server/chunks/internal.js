import{H as v,g as N,a as P,i as b,b as M,C,c as A,d as D,h as H,e as I,f as j,j as Y,k as q,l as F,m as U,p as z,n as B,o as V,s as G,L as W,q as E,r as J,t as K,u as Q,v as X,w as Z,x as $,y as tt}from"./index.js";import{a as et,r as O,h as g}from"./events.js";import"clsx";import"./environment.js";import"./server.js";let nt={};function vt(e){}function yt(e){nt=e}function S(e){console.warn("https://svelte.dev/e/hydration_mismatch")}let y=!1;function p(e){y=e}let u;function w(e){if(e===null)throw S(),v;return u=e}function st(){return w(N(u))}function rt(e,t){var r=P;r.nodes_start===null&&(r.nodes_start=e,r.nodes_end=t)}function T(e,t){return L(e,t)}function at(e,t){b(),t.intro=t.intro??!1;const r=t.target,i=y,a=u;try{for(var n=M(r);n&&(n.nodeType!==C||n.data!==A);)n=N(n);if(!n)throw v;p(!0),w(n),st();const s=L(e,{...t,anchor:n});if(u===null||u.nodeType!==C||u.data!==D)throw S(),v;return p(!1),s}catch(s){if(s===v)return t.recover===!1&&H(),b(),I(r),p(!1),T(e,t);throw s}finally{p(i),w(a)}}const h=new Map;function L(e,{target:t,anchor:r,props:i={},events:a,context:n,intro:s=!0}){b();var o=new Set,f=l=>{for(var d=0;d<l.length;d++){var c=l[d];if(!o.has(c)){o.add(c);var k=q(c);t.addEventListener(c,g,{passive:k});var R=h.get(c);R===void 0?(document.addEventListener(c,g,{passive:k}),h.set(c,1)):h.set(c,R+1)}}};f(j(et)),O.add(f);var m=void 0,_=Y(()=>{var l=r??t.appendChild(F());return U(()=>{if(n){z({});var d=B;d.c=n}a&&(i.$$events=a),y&&rt(l,null),m=e(l,i)||{},y&&(P.nodes_end=u),n&&V()}),()=>{for(var d of o){t.removeEventListener(d,g);var c=h.get(d);--c===0?(document.removeEventListener(d,g),h.delete(d)):h.set(d,c)}O.delete(f),l!==r&&l.parentNode?.removeChild(l)}});return x.set(m,_),m}let x=new WeakMap;function it(e,t){const r=x.get(e);return r?(x.delete(e),r(t)):Promise.resolve()}function ot(e){return class extends lt{constructor(t){super({component:e,...t})}}}class lt{#e;#t;constructor(t){var r=new Map,i=(n,s)=>{var o=Q(s,!1,!1);return r.set(n,o),o};const a=new Proxy({...t.props||{},$$events:{}},{get(n,s){return E(r.get(s)??i(s,Reflect.get(n,s)))},has(n,s){return s===W?!0:(E(r.get(s)??i(s,Reflect.get(n,s))),Reflect.has(n,s))},set(n,s,o){return G(r.get(s)??i(s,o),o),Reflect.set(n,s,o)}});this.#t=(t.hydrate?at:T)(t.component,{target:t.target,anchor:t.anchor,props:a,context:t.context,intro:t.intro??!1,recover:t.recover}),(!t?.props?.$$host||t.sync===!1)&&J(),this.#e=a.$$events;for(const n of Object.keys(this.#t))n==="$set"||n==="$destroy"||n==="$on"||K(this,n,{get(){return this.#t[n]},set(s){this.#t[n]=s},enumerable:!0});this.#t.$set=n=>{Object.assign(a,n)},this.#t.$destroy=()=>{it(this.#t)}}$set(t){this.#t.$set(t)}$on(t,r){this.#e[t]=this.#e[t]||[];const i=(...a)=>r.call(this,...a);return this.#e[t].push(i),()=>{this.#e[t]=this.#e[t].filter(a=>a!==i)}}$destroy(){this.#t.$destroy()}}let dt=null;function bt(e){dt=e}function wt(e){}function ct(e){const t=ot(e),r=(i,{context:a}={})=>{const n=X(e,{props:i,context:a});return{css:{code:"",map:null},head:n.head,html:n.body}};return t.render=r,t}function ut(e,t){Z();let{stores:r,page:i,constructors:a,components:n=[],form:s,data_0:o=null,data_1:f=null}=t;$("__svelte__",r),r.page.set(i);const m=a[1];if(a[1]){e.out.push("<!--[-->");const _=a[0];e.out.push("<!---->"),_(e,{data:o,form:s,params:i.params,children:l=>{l.out.push("<!---->"),m(l,{data:f,form:s,params:i.params}),l.out.push("<!---->")},$$slots:{default:!0}}),e.out.push("<!---->")}else{e.out.push("<!--[!-->");const _=a[0];e.out.push("<!---->"),_(e,{data:o,form:s,params:i.params}),e.out.push("<!---->")}e.out.push("<!--]--> "),e.out.push("<!--[!-->"),e.out.push("<!--]-->"),tt()}const ht=ct(ut),xt={app_template_contains_nonce:!1,async:!1,csp:{mode:"auto",directives:{"upgrade-insecure-requests":!1,"block-all-mixed-content":!1},reportOnly:{"upgrade-insecure-requests":!1,"block-all-mixed-content":!1}},csrf_check_origin:!0,csrf_trusted_origins:[],embedded:!1,env_public_prefix:"PUBLIC_",env_private_prefix:"",hash_routing:!0,hooks:null,preload_strategy:"modulepreload",root:ht,service_worker:!1,service_worker_options:void 0,templates:{app:({head:e,body:t,assets:r,nonce:i,env:a})=>`<!doctype html>
<html lang="en">
	<head>
		<meta charset="utf-8" />
		<link rel="icon" type="image/svg+xml" href="/favicon.svg" />
		<meta name="viewport" content="width=device-width, initial-scale=1" />
		`+e+`
	</head>
	<body data-sveltekit-preload-data="hover">
		<div style="display: contents">`+t+`</div>
	</body>
</html>
`,error:({status:e,message:t})=>`<!doctype html>
<html lang="en">
	<head>
		<meta charset="utf-8" />
		<title>`+t+`</title>

		<style>
			body {
				--bg: white;
				--fg: #222;
				--divider: #ccc;
				background: var(--bg);
				color: var(--fg);
				font-family:
					system-ui,
					-apple-system,
					BlinkMacSystemFont,
					'Segoe UI',
					Roboto,
					Oxygen,
					Ubuntu,
					Cantarell,
					'Open Sans',
					'Helvetica Neue',
					sans-serif;
				display: flex;
				align-items: center;
				justify-content: center;
				height: 100vh;
				margin: 0;
			}

			.error {
				display: flex;
				align-items: center;
				max-width: 32rem;
				margin: 0 1rem;
			}

			.status {
				font-weight: 200;
				font-size: 3rem;
				line-height: 1;
				position: relative;
				top: -0.05rem;
			}

			.message {
				border-left: 1px solid var(--divider);
				padding: 0 0 0 1rem;
				margin: 0 0 0 1rem;
				min-height: 2.5rem;
				display: flex;
				align-items: center;
			}

			.message h1 {
				font-weight: 400;
				font-size: 1em;
				margin: 0;
			}

			@media (prefers-color-scheme: dark) {
				body {
					--bg: #222;
					--fg: #ddd;
					--divider: #666;
				}
			}
		</style>
	</head>
	<body>
		<div class="error">
			<span class="status">`+e+`</span>
			<div class="message">
				<h1>`+t+`</h1>
			</div>
		</div>
	</body>
</html>
`},version_hash:"1iyrivo"};async function kt(){return{handle:void 0,handleFetch:void 0,handleError:void 0,handleValidationError:void 0,init:void 0,reroute:void 0,transport:void 0}}export{yt as a,bt as b,wt as c,kt as g,xt as o,nt as p,dt as r,vt as s};

import{H as v,g as N,a as P,i as b,b as M,C,c as A,d as D,h as H,e as I,f as j,j as q,k as Y,l as F,m as U,p as z,n as B,o as V,s as G,L as W,q as E,r as J,t as K,u as Q,v as X,w as Z,x as $,y as tt}from"./index.js";import{a as et,r as O,h as g}from"./events.js";import"clsx";import"./environment.js";import"./server.js";let nt={};function vt(e){}function yt(e){nt=e}function S(e){console.warn("https://svelte.dev/e/hydration_mismatch")}let y=!1;function p(e){y=e}let u;function x(e){if(e===null)throw S(),v;return u=e}function st(){return x(N(u))}function rt(e,t){var s=P;s.nodes_start===null&&(s.nodes_start=e,s.nodes_end=t)}function T(e,t){return L(e,t)}function at(e,t){b(),t.intro=t.intro??!1;const s=t.target,i=y,a=u;try{for(var n=M(s);n&&(n.nodeType!==C||n.data!==A);)n=N(n);if(!n)throw v;p(!0),x(n),st();const r=L(e,{...t,anchor:n});if(u===null||u.nodeType!==C||u.data!==D)throw S(),v;return p(!1),r}catch(r){if(r===v)return t.recover===!1&&H(),b(),I(s),p(!1),T(e,t);throw r}finally{p(i),x(a)}}const h=new Map;function L(e,{target:t,anchor:s,props:i={},events:a,context:n,intro:r=!0}){b();var o=new Set,f=l=>{for(var d=0;d<l.length;d++){var c=l[d];if(!o.has(c)){o.add(c);var k=Y(c);t.addEventListener(c,g,{passive:k});var R=h.get(c);R===void 0?(document.addEventListener(c,g,{passive:k}),h.set(c,1)):h.set(c,R+1)}}};f(j(et)),O.add(f);var m=void 0,_=q(()=>{var l=s??t.appendChild(F());return U(()=>{if(n){z({});var d=B;d.c=n}a&&(i.$$events=a),y&&rt(l,null),m=e(l,i)||{},y&&(P.nodes_end=u),n&&V()}),()=>{for(var d of o){t.removeEventListener(d,g);var c=h.get(d);--c===0?(document.removeEventListener(d,g),h.delete(d)):h.set(d,c)}O.delete(f),l!==s&&l.parentNode?.removeChild(l)}});return w.set(m,_),m}let w=new WeakMap;function it(e,t){const s=w.get(e);return s?(w.delete(e),s(t)):Promise.resolve()}function ot(e){return class extends lt{constructor(t){super({component:e,...t})}}}class lt{#e;#t;constructor(t){var s=new Map,i=(n,r)=>{var o=Q(r,!1,!1);return s.set(n,o),o};const a=new Proxy({...t.props||{},$$events:{}},{get(n,r){return E(s.get(r)??i(r,Reflect.get(n,r)))},has(n,r){return r===W?!0:(E(s.get(r)??i(r,Reflect.get(n,r))),Reflect.has(n,r))},set(n,r,o){return G(s.get(r)??i(r,o),o),Reflect.set(n,r,o)}});this.#t=(t.hydrate?at:T)(t.component,{target:t.target,anchor:t.anchor,props:a,context:t.context,intro:t.intro??!1,recover:t.recover}),(!t?.props?.$$host||t.sync===!1)&&J(),this.#e=a.$$events;for(const n of Object.keys(this.#t))n==="$set"||n==="$destroy"||n==="$on"||K(this,n,{get(){return this.#t[n]},set(r){this.#t[n]=r},enumerable:!0});this.#t.$set=n=>{Object.assign(a,n)},this.#t.$destroy=()=>{it(this.#t)}}$set(t){this.#t.$set(t)}$on(t,s){this.#e[t]=this.#e[t]||[];const i=(...a)=>s.call(this,...a);return this.#e[t].push(i),()=>{this.#e[t]=this.#e[t].filter(a=>a!==i)}}$destroy(){this.#t.$destroy()}}let dt=null;function bt(e){dt=e}function xt(e){}function ct(e){const t=ot(e),s=(i,{context:a}={})=>{const n=X(e,{props:i,context:a});return{css:{code:"",map:null},head:n.head,html:n.body}};return t.render=s,t}function ut(e,t){Z();let{stores:s,page:i,constructors:a,components:n=[],form:r,data_0:o=null,data_1:f=null}=t;$("__svelte__",s),s.page.set(i);const m=a[1];if(a[1]){e.out.push("<!--[-->");const _=a[0];e.out.push("<!---->"),_(e,{data:o,form:r,params:i.params,children:l=>{l.out.push("<!---->"),m(l,{data:f,form:r,params:i.params}),l.out.push("<!---->")},$$slots:{default:!0}}),e.out.push("<!---->")}else{e.out.push("<!--[!-->");const _=a[0];e.out.push("<!---->"),_(e,{data:o,form:r,params:i.params}),e.out.push("<!---->")}e.out.push("<!--]--> "),e.out.push("<!--[!-->"),e.out.push("<!--]-->"),tt()}const ht=ct(ut),wt={app_template_contains_nonce:!1,async:!1,csp:{mode:"auto",directives:{"upgrade-insecure-requests":!1,"block-all-mixed-content":!1},reportOnly:{"upgrade-insecure-requests":!1,"block-all-mixed-content":!1}},csrf_check_origin:!0,csrf_trusted_origins:[],embedded:!1,env_public_prefix:"PUBLIC_",env_private_prefix:"",hash_routing:!0,hooks:null,preload_strategy:"modulepreload",root:ht,service_worker:!1,service_worker_options:void 0,templates:{app:({head:e,body:t,assets:s,nonce:i,env:a})=>`<!doctype html>
<html lang="en">
	<head>
		<meta charset="utf-8" />
		<link rel="icon" type="image/svg+xml" href="`+s+`/favicon.svg" />
		<!-- <link rel="shortcut icon" type="image/svg+xml" href="`+s+`/favicon.svg" /> -->
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
`},version_hash:"x3xq1e"};async function kt(){return{handle:void 0,handleFetch:void 0,handleError:void 0,handleValidationError:void 0,init:void 0,reroute:void 0,transport:void 0}}export{yt as a,bt as b,xt as c,kt as g,wt as o,nt as p,dt as r,vt as s};

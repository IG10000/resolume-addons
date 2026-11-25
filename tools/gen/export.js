const fs=require('fs');
const html=fs.readFileSync(process.argv[2],'utf8');
const js=html.match(/<script>([\s\S]*)<\/script>/)[1];
const a=js.indexOf('const PRELUDE'); const b=js.indexOf('/* ---------- GL setup');
const code=js.slice(a,b);
const out=new Function(code+'\nreturn {PRELUDE,MAIN,SOURCES,COPY,EFFECTS};')();
fs.writeFileSync(process.argv[3],JSON.stringify(out,null,1));
console.log(out.EFFECTS.length);
for(const e of out.EFFECTS) console.log(e.cat.padEnd(12),e.id.padEnd(16),e.params.map(p=>p[0]+'='+p[1]).join(' | '));

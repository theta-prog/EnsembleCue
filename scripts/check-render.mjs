// Optional development-only Node.js check; not used by the application.
import fs from 'node:fs';
import path from 'node:path';
const folder = process.argv[2] || 'results';
function wav(name) {
  const b = fs.readFileSync(path.join(folder, name));
  if (b.toString('ascii',0,4)!=='RIFF' || b.toString('ascii',8,12)!=='WAVE') throw Error('Invalid WAV header');
  let fmt, data;
  for(let i=12;i+8<=b.length;) {
    const size=b.readUInt32LE(i+4), end=i+8+size;
    if(end>b.length) throw Error('Truncated WAV chunk');
    const id=b.toString('ascii',i,i+4);
    if(id==='fmt ') fmt=b.subarray(i+8,end);
    if(id==='data') data=b.subarray(i+8,end);
    i=end+(size&1);
  }
  if(!fmt || !data || fmt.readUInt16LE(0)!==1 || fmt.readUInt16LE(2)!==1 || fmt.readUInt16LE(14)!==24) throw Error('Expected PCM24 mono');
  let peak=0, energy=0;
  const samples=new Float64Array(data.length/3);
  for(let i=0;i<samples.length;i++) {
    const v=data.readIntLE(i*3,3)/8388608; samples[i]=v; peak=Math.max(peak,Math.abs(v)); energy+=v*v;
  }
  if(peak<.001 || peak>=.95) throw Error('Silent or clipped render');
  return {samples,rate:fmt.readUInt32LE(4),peak,rms:Math.sqrt(energy/samples.length)};
}
const direct=wav('demo-band.wav'), replay=wav('wav-replay.wav');
if(direct.samples.length!==replay.samples.length) throw Error('Replay length differs');
let difference=0; for(let i=0;i<direct.samples.length;i++) difference=Math.max(difference,Math.abs(direct.samples[i]-replay.samples[i]));
if(difference>.01) throw Error('WAV re-input changed rendered audio substantially');
function readMidi(name) {
  const b=fs.readFileSync(path.join(folder,name));
  if(b.toString('ascii',0,4)!=='MThd' || b.readUInt16BE(12)!==960) throw Error('Invalid MIDI header');
  let offset=14, on=0, off=0; const sounding=new Map(), channels={};
  const vlq=()=>{let n=0,c,count=0; do {if(offset>=b.length || ++count>4) throw Error('Invalid VLQ'); c=b[offset++]; n=(n<<7)|(c&127);}while(c&128); return n;};
  for(let track=0;track<b.readUInt16BE(10);track++) {
    if(b.toString('ascii',offset,offset+4)!=='MTrk') throw Error('Missing track');
    const end=offset+8+b.readUInt32BE(offset+4); offset+=8; let status=0;
    while(offset<end) {
      vlq(); if(b[offset]&128) status=b[offset++];
      if(status===255) {offset++; const n=vlq(); offset+=n;continue;}
      if(status===240 || status===247) {const n=vlq();offset+=n;continue;}
      const type=status>>4, pitch=b[offset++];
      if(type===12 || type===13) continue;
      const velocity=b[offset++], key=((status&15)<<8)|pitch;
      if(type===9 && velocity>0) {on++;channels[(status&15)+1]=(channels[(status&15)+1]||0)+1;sounding.set(key,(sounding.get(key)||0)+1);}
      if(type===8 || (type===9 && velocity===0)) {off++;sounding.set(key,(sounding.get(key)||0)-1);}
    }
    if(offset!==end) throw Error('Invalid MIDI track length');
  }
  if(on<20 || on!==off || [...sounding.values()].some(n=>n!==0)) throw Error('Unbalanced MIDI notes');
  if(process.argv.includes('--ensemble') && ![1,2,3].every(channel=>channels[channel]>0)) throw Error('Missing instrument MIDI channel');
  return {noteOn:on,noteOff:off,channels};
}
const result={seconds:direct.samples.length/direct.rate,peak:direct.peak,rms:direct.rms,maxReplayDifference:difference,midi:readMidi('demo-band.mid')};
fs.writeFileSync(path.join(folder,'render-check.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result,null,2));

function up(){
var f=document.getElementById('f').files[0];
if(!f){return;}
document.getElementById('status').textContent='enviando...';
fetch('/upload/'+encodeURIComponent(f.name),{method:'POST',body:f})
.then(function(r){return r.text();})
.then(function(t){document.getElementById('status').textContent=t;location.reload();})
.catch(function(e){document.getElementById('status').textContent='erro: '+e;});
}
function del(btn){
var encoded=btn.getAttribute('data-name');
if(!confirm('Apagar '+decodeURIComponent(encoded)+'?')){return;}
fetch('/delete/'+encoded,{method:'POST'})
.then(function(r){return r.text();})
.then(function(){location.reload();})
.catch(function(e){alert('erro: '+e);});
}

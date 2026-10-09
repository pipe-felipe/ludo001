function up() {
    var f = document.getElementById('f').files[0];
    if (!f) { return; }
    document.getElementById('status').textContent = 'enviando...';
    fetch('/upload/' + encodeURIComponent(f.name), { method: 'POST', body: f })
        .then(function (r) { return r.text(); })
        .then(function (t) { document.getElementById('status').textContent = t; location.reload(); })
        .catch(function (e) { document.getElementById('status').textContent = 'erro: ' + e; });
}

function del(btn) {
    var encoded = btn.getAttribute('data-name');
    if (!confirm('Apagar ' + decodeURIComponent(encoded) + '?')) { return; }
    fetch('/delete/' + encoded, { method: 'POST' })
        .then(function (r) { return r.text(); })
        .then(function () { location.reload(); })
        .catch(function (e) { alert('erro: ' + e); });
}

function encodePath(relPath) {
    return relPath.split('/').map(encodeURIComponent).join('/');
}

function upDir() {
    var files = document.getElementById('d').files;
    if (!files || files.length === 0) { return; }
    var i = 0;
    function next() {
        if (i >= files.length) {
            document.getElementById('status').textContent = 'pasta enviada (' + files.length + ' arquivos)';
            location.reload();
            return;
        }
        var file = files[i];
        var rel = file.webkitRelativePath || file.name;
        document.getElementById('status').textContent = 'enviando ' + (i + 1) + '/' + files.length + '...';
        fetch('/upload/' + encodePath(rel), { method: 'POST', body: file })
            .then(function (r) {
                if (!r.ok) { throw new Error('falhou em ' + rel); }
                i++;
                next();
            })
            .catch(function (e) { document.getElementById('status').textContent = 'erro: ' + e; });
    }
    next();
}

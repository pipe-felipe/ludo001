function formatSize(bytes) {
    if (bytes < 1024) { return bytes + ' B'; }
    if (bytes < 1024 * 1024) { return (bytes / 1024).toFixed(1) + ' KB'; }
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function setStatus(text, isError) {
    var el = document.getElementById('status');
    el.textContent = text;
    el.classList.toggle('error', !!isError);
}

// percent === null means "unknown": the bar slides back and forth instead.
function setProgress(percent) {
    var fill = document.getElementById('fill');
    document.getElementById('progress').hidden = false;
    fill.classList.toggle('indeterminate', percent === null);
    fill.style.width = percent === null ? '' : percent + '%';
}

// Shows what was chosen in an upload box and enables its send button.
function picked(input) {
    var box = input.closest('.upload');
    var hint = box.querySelector('.drop-hint');
    var files = input.files;
    var chosen = files && files.length > 0;
    if (!hint.getAttribute('data-empty')) { hint.setAttribute('data-empty', hint.textContent); }
    box.classList.toggle('has-file', chosen);
    box.querySelector('button').disabled = !chosen;
    if (!chosen) {
        hint.textContent = hint.getAttribute('data-empty');
    } else if (input.hasAttribute('webkitdirectory')) {
        hint.textContent = (files[0].webkitRelativePath || files[0].name).split('/')[0] +
            ' (' + files.length + ' arquivos)';
    } else {
        hint.textContent = files[0].name + ' (' + formatSize(files[0].size) + ')';
    }
}

function setBusy(busy) {
    var buttons = document.querySelectorAll('.upload button');
    for (var i = 0; i < buttons.length; i++) { buttons[i].disabled = busy; }
    if (!busy) {
        picked(document.getElementById('f'));
        picked(document.getElementById('d'));
    }
}

function fail(e) {
    document.getElementById('progress').hidden = true;
    setStatus('erro: ' + (e && e.message ? e.message : e), true);
    setBusy(false);
}

function up() {
    var f = document.getElementById('f').files[0];
    if (!f) { return; }
    setBusy(true);
    setProgress(null);
    setStatus('enviando ' + f.name + '...');
    fetch('/upload/' + encodeURIComponent(f.name), { method: 'POST', body: f })
        .then(function (r) {
            if (!r.ok) { throw new Error('falhou ao enviar ' + f.name); }
            location.reload();
        })
        .catch(fail);
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
    setBusy(true);
    var i = 0;
    function next() {
        if (i >= files.length) {
            setProgress(100);
            setStatus('pasta enviada (' + files.length + ' arquivos)');
            location.reload();
            return;
        }
        var file = files[i];
        var rel = file.webkitRelativePath || file.name;
        setProgress(i * 100 / files.length);
        setStatus('enviando ' + (i + 1) + '/' + files.length + '...');
        fetch('/upload/' + encodePath(rel), { method: 'POST', body: file })
            .then(function (r) {
                if (!r.ok) { throw new Error('falhou em ' + rel); }
                i++;
                next();
            })
            .catch(fail);
    }
    next();
}

// Lowercase and strip accents, so "relatorio" finds "Relatório".
function plain(text) {
    return text.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase();
}

// Hides the rows of the current folder whose name lacks any of the typed
// words. The whole folder is already on the page, so nothing is requested.
function filterList(query) {
    var terms = plain(query).split(/\s+/).filter(Boolean);
    var rows = document.querySelectorAll('ul li');
    var shown = 0;
    var lastShown = null;
    for (var i = 0; i < rows.length; i++) {
        var name = plain(rows[i].querySelector('a').textContent);
        var match = terms.every(function (t) { return name.indexOf(t) >= 0; });
        rows[i].hidden = !match;
        rows[i].classList.remove('last');
        if (match) {
            shown++;
            lastShown = rows[i];
        }
    }
    if (lastShown) { lastShown.classList.add('last'); } // li:last-child may be a hidden row
    document.getElementById('no-match').hidden = shown > 0 || rows.length === 0;
    document.querySelector('ul').scrollTop = 0;
}

// Dropping a file on the file box works like choosing it. (Folders are not
// handled: a dropped folder arrives as an entry, not as a list of files.)
(function () {
    var input = document.getElementById('f');
    var drop = input.parentNode;
    drop.addEventListener('dragover', function (e) {
        e.preventDefault();
        drop.classList.add('over');
    });
    drop.addEventListener('dragleave', function () { drop.classList.remove('over'); });
    drop.addEventListener('drop', function (e) {
        e.preventDefault();
        drop.classList.remove('over');
        if (e.dataTransfer.files.length > 0) {
            input.files = e.dataTransfer.files;
            picked(input);
        }
    });
    // A reload can leave a chosen file in the input; reflect it in the box.
    picked(input);
    picked(document.getElementById('d'));
    // The search box only makes sense when there is something to search; a
    // reload can also leave text in it.
    if (document.querySelector('ul li')) {
        document.getElementById('search').hidden = false;
        filterList(document.getElementById('q').value);
    }
})();

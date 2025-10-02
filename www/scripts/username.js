document.addEventListener('DOMContentLoaded', function () {
    const form = document.getElementById('username-form');
    if (!form) {
        console.log("Form not found!");
        return;
    }
    form.addEventListener('submit', function (e) {
        e.preventDefault();
        const username = document.getElementById('username').value.trim();
        if (!username) return;

        fetch('/username', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username })
        })
        .then(res => {
            if (!res.ok) throw new Error('Failed to set username');
            return res.json().catch(() => ({}));
        })
        .then(() => {
            window.location.href = '/';
        })
        .catch(err => {
            console.error(err);
            alert('Failed to set username, please try again.');
        });
    });
});
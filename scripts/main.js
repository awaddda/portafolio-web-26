/* ── main.js ── Santino Awada Portfolio ── */

// ── SCROLL REVEAL ──────────────────────────
const revealObserver = new IntersectionObserver(
    (entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.classList.add('visible');
            }
        });
    },
    { threshold: 0.08, rootMargin: '0px 0px -40px 0px' }
);

document.querySelectorAll('.reveal').forEach(el => revealObserver.observe(el));

// ── NAV SCROLL EFFECT ──────────────────────
const navbar = document.getElementById('navbar');
window.addEventListener('scroll', () => {
    navbar.classList.toggle('scrolled', window.scrollY > 20);
}, { passive: true });

// ── THEME TOGGLE ───────────────────────────
const btnTema = document.getElementById('btn-tema');

function applyTheme(mode) {
    document.body.classList.toggle('modo-claro', mode === 'claro');
    btnTema.textContent = mode === 'claro' ? '☀︎' : '☽';
    localStorage.setItem('tema', mode);
}

const savedTheme = localStorage.getItem('tema') || 'oscuro';
applyTheme(savedTheme);

btnTema.addEventListener('click', () => {
    const current = document.body.classList.contains('modo-claro') ? 'claro' : 'oscuro';
    applyTheme(current === 'claro' ? 'oscuro' : 'claro');
});

// ── DIALOG: btn-diagnostico ────────────────
const btnDiag = document.getElementById('btn-diagnostico');
const modalInfo = document.getElementById('modal-info');
const btnCerrar = document.getElementById('btn-cerrar');

if (btnDiag) btnDiag.addEventListener('click', () => modalInfo.showModal());
if (btnCerrar) btnCerrar.addEventListener('click', () => modalInfo.close());

// Close dialogs on backdrop click
document.querySelectorAll('dialog').forEach(dialog => {
    dialog.addEventListener('click', e => {
        const rect = dialog.getBoundingClientRect();
        const clickedOutside =
            e.clientX < rect.left || e.clientX > rect.right ||
            e.clientY < rect.top  || e.clientY > rect.bottom;
        if (clickedOutside) dialog.close();
    });
});

// ── SMOOTH NAV ACTIVE STATE ─────────────────
const sections = document.querySelectorAll('section[id]');
const navLinks = document.querySelectorAll('nav a');

const sectionObserver = new IntersectionObserver(
    (entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                navLinks.forEach(link => {
                    link.style.color = '';
                    link.style.background = '';
                });
                const active = document.querySelector(`nav a[href="#${entry.target.id}"]`);
                if (active) {
                    active.style.color = 'var(--accent)';
                }
            }
        });
    },
    { threshold: 0.4 }
);

sections.forEach(s => sectionObserver.observe(s));
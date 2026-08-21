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
    // IMPORTANTE: Usar 'modo-oscuro' que es lo que está definido en el CSS
    if (mode === 'oscuro') {
        document.body.classList.add('modo-oscuro');
        btnTema.textContent = '☀︎'; // ☀︎ = cambiar a claro
    } else {
        document.body.classList.remove('modo-oscuro');
        btnTema.textContent = '☽'; // ☽ = cambiar a oscuro
    }
    localStorage.setItem('tema', mode);
}

// Cargar tema guardado o usar 'oscuro' por defecto
const savedTheme = localStorage.getItem('tema') || 'oscuro';
applyTheme(savedTheme);

// Toggle al hacer click
btnTema.addEventListener('click', () => {
    const isDark = document.body.classList.contains('modo-oscuro');
    // Si está oscuro → cambiar a claro, si está claro → cambiar a oscuro
    applyTheme(isDark ? 'claro' : 'oscuro');
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
                    active.style.color = 'var(--neo-accent)';
                }
            }
        });
    },
    { threshold: 0.4 }
);

sections.forEach(s => sectionObserver.observe(s));

// ── Modales de presupuesto ──
function abrirModal(id) {
    const overlay = document.getElementById(id);
    if (!overlay) return;
    overlay.classList.add('activo');
    document.body.style.overflow = 'hidden';
    // Cerrar con Escape
    overlay._onKeyDown = (e) => { if (e.key === 'Escape') cerrarModal(id); };
    document.addEventListener('keydown', overlay._onKeyDown);
}

function cerrarModal(id) {
    const overlay = document.getElementById(id);
    if (!overlay) return;
    overlay.classList.remove('activo');
    document.body.style.overflow = '';
    if (overlay._onKeyDown) {
        document.removeEventListener('keydown', overlay._onKeyDown);
    }
}

// Cerrar al hacer click fuera del panel
document.querySelectorAll('.modal-overlay').forEach(overlay => {
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) cerrarModal(overlay.id);
    });
});
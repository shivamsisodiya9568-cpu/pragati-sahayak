// Pragati Sahayak Firebase Logic
// Works on GitHub Pages with Firebase Auth + Firestore.

// ---------- Helpers ----------
function psGet(id) {
  return document.getElementById(id);
}

function psToast(message, type = "success") {
  let box = document.getElementById("ps-toast-box");
  if (!box) {
    box = document.createElement("div");
    box.id = "ps-toast-box";
    box.style.cssText = "position:fixed;right:18px;bottom:18px;z-index:99999;display:grid;gap:10px;max-width:360px";
    document.body.appendChild(box);
  }

  const item = document.createElement("div");
  item.textContent = message;
  item.style.cssText = `
    padding:14px 18px;border-radius:18px;color:white;font-weight:700;
    background:${type === "error" ? "#dc2626" : "#0891b2"};
    box-shadow:0 18px 45px rgba(0,0,0,.22);font-family:Inter,system-ui,sans-serif;
  `;
  box.appendChild(item);
  setTimeout(() => item.remove(), 4200);
}

function psFormToObject(form) {
  return Object.fromEntries(
    [...new FormData(form).entries()].map(([key, value]) => [key, typeof value === "string" ? value.trim() : value])
  );
}

// ---------- Main Website Lead Form ----------
window.submitLeadForm = async function submitLeadForm(event) {
  event.preventDefault();

  const name = psGet("name")?.value.trim();
  const business = psGet("business")?.value.trim();
  const phone = psGet("phone")?.value.trim();
  const status = psGet("lead-status");

  if (!name || !business || !phone) {
    if (status) status.textContent = "Please fill name, business and phone number.";
    psToast("Please complete all required fields.", "error");
    return;
  }

  try {
    if (status) status.textContent = "Submitting...";
    await db.collection("leads").add({
      name,
      business,
      phone,
      source: "website-lead-form",
      status: "new",
      createdAt: firebase.firestore.FieldValue.serverTimestamp()
    });

    event.target.reset();
    if (status) status.textContent = "Request submitted successfully. We will contact you soon.";
    psToast("Proposal request saved in Firebase.");
  } catch (error) {
    console.error(error);
    if (status) status.textContent = error.message || "Something went wrong.";
    psToast(error.message || "Something went wrong.", "error");
  }
};

// ---------- Auth Page ----------
async function psSignup(event) {
  event.preventDefault();
  const form = event.currentTarget;
  const data = psFormToObject(form);

  if (data.password !== data.confirmPassword) {
    psToast("Password and confirm password do not match.", "error");
    return;
  }

  try {
    const result = await auth.createUserWithEmailAndPassword(data.email, data.password);
    await db.collection("users").doc(result.user.uid).set({
      uid: result.user.uid,
      name: data.name,
      email: data.email,
      phone: data.phone || "",
      createdAt: firebase.firestore.FieldValue.serverTimestamp()
    });

    psToast("Account created successfully.");
    setTimeout(() => location.href = "dashboard.html", 800);
  } catch (error) {
    console.error(error);
    psToast(error.message, "error");
  }
}

async function psLogin(event) {
  event.preventDefault();
  const form = event.currentTarget;
  const data = psFormToObject(form);

  try {
    await auth.signInWithEmailAndPassword(data.email, data.password);
    psToast("Login successful.");
    setTimeout(() => location.href = "dashboard.html", 700);
  } catch (error) {
    console.error(error);
    psToast(error.message, "error");
  }
}

async function psResetPassword() {
  const email = prompt("Enter your registered email:");
  if (!email) return;

  try {
    await auth.sendPasswordResetEmail(email.trim());
    psToast("Password reset email sent.");
  } catch (error) {
    psToast(error.message, "error");
  }
}

async function psLogout() {
  await auth.signOut();
  psToast("Logged out.");
  setTimeout(() => location.href = "auth.html", 700);
}

window.psSignup = psSignup;
window.psLogin = psLogin;
window.psResetPassword = psResetPassword;
window.psLogout = psLogout;

// ---------- Dashboard / Form Board ----------
async function psLoadDashboard() {
  const board = psGet("ps-leads-board");
  if (!board) return;

  auth.onAuthStateChanged(async (user) => {
    if (!user) {
      location.href = "auth.html";
      return;
    }

    const userEmail = psGet("ps-user-email");
    if (userEmail) userEmail.textContent = user.email;

    try {
      board.innerHTML = `<div class="text-zinc-500">Loading form submissions...</div>`;

      const snapshot = await db.collection("leads").orderBy("createdAt", "desc").limit(100).get();

      const total = psGet("ps-total-leads");
      if (total) total.textContent = snapshot.size;

      if (snapshot.empty) {
        board.innerHTML = `<div class="rounded-3xl border border-dashed border-zinc-300 p-8 text-center text-zinc-500">No form submissions yet.</div>`;
        return;
      }

      board.innerHTML = "";
      snapshot.forEach((doc) => {
        const lead = doc.data();
        const created = lead.createdAt?.toDate ? lead.createdAt.toDate().toLocaleString() : "Just now";

        const card = document.createElement("article");
        card.className = "rounded-3xl bg-white border border-zinc-200 p-6 shadow-sm";
        card.innerHTML = `
          <div class="flex flex-col md:flex-row md:items-start md:justify-between gap-4">
            <div>
              <h3 class="text-xl font-bold text-zinc-950">${lead.name || "No name"}</h3>
              <p class="text-zinc-500 mt-1">${lead.business || "No business name"}</p>
              <p class="text-zinc-700 mt-4"><i class="fa-solid fa-phone text-cyan-500"></i> ${lead.phone || "No phone"}</p>
              <p class="text-xs text-zinc-400 mt-4">Submitted: ${created}</p>
            </div>
            <span class="px-4 py-2 rounded-full bg-cyan-50 text-cyan-700 text-sm font-semibold">${lead.status || "new"}</span>
          </div>
        `;
        board.appendChild(card);
      });
    } catch (error) {
      console.error(error);
      board.innerHTML = `<div class="rounded-3xl bg-red-50 text-red-700 p-6">${error.message}</div>`;
    }
  });
}

document.addEventListener("DOMContentLoaded", psLoadDashboard);

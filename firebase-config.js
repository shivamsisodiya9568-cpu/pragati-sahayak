// Pragati Sahayak Firebase Config
// Step 1: Firebase Console -> Project Settings -> General -> Your apps -> Web app
// Step 2: Copy firebaseConfig values and replace the placeholders below.
// Step 3: Authentication me Email/Password enable karo.
// Step 4: Firestore Database create karo.

const firebaseConfig = {
  apiKey: "PASTE_YOUR_API_KEY_HERE",
  authDomain: "PASTE_YOUR_AUTH_DOMAIN_HERE",
  projectId: "PASTE_YOUR_PROJECT_ID_HERE",
  storageBucket: "PASTE_YOUR_STORAGE_BUCKET_HERE",
  messagingSenderId: "PASTE_YOUR_MESSAGING_SENDER_ID_HERE",
  appId: "PASTE_YOUR_APP_ID_HERE"
};

firebase.initializeApp(firebaseConfig);

const db = firebase.firestore();
const auth = firebase.auth();

// Optional: yahan apna admin email likh sakte ho.
// Example: const ADMIN_EMAILS = ["yourmail@gmail.com"];
const ADMIN_EMAILS = [];

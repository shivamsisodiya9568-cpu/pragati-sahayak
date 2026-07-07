# Pragati Sahayak Firebase Setup

Is project me Firebase login/signup, create account, lead form saving aur form board add kar diya gaya hai.

## 1. Firebase Project
Firebase Console me new project banao.

## 2. Authentication
Security / Authentication -> Get Started -> Add provider -> Email/Password -> Enable -> Save.

## 3. Firestore Database
Firestore Database -> Create Database -> Test mode (learning ke liye) -> Create.

## 4. Firebase Config
Project Settings -> General -> Your apps -> Web app -> Config copy karo.

`firebase-config.js` file me placeholders replace karo:

- apiKey
- authDomain
- projectId
- storageBucket
- messagingSenderId
- appId

## 5. GitHub Pages
Saari files GitHub repo me upload karo.
Settings -> Pages -> Deploy from branch -> main / root -> Save.

## Pages Added

- `auth.html` = Login + Create New Account + Forgot Password
- `dashboard.html` = Form Fill Board / Client Requests
- `firebase-config.js` = Firebase connection
- `pragati-firebase.js` = Login/signup/form saving logic

## Firestore Collections

- `users` = Signup users
- `leads` = Website proposal/contact form data

## Important
GitHub Pages Node.js run nahi karta. Ye setup pure HTML + CSS + JS + Firebase ke saath compatible hai.

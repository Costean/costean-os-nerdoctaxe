import { mount } from 'svelte';
import './app.css';
import App from './App.svelte';
document.documentElement.dataset.theme = localStorage.getItem('costean.theme') || 'dark';
mount(App, { target: document.getElementById('app') });
if ('serviceWorker' in navigator && window.isSecureContext) navigator.serviceWorker.register('sw.js').catch(() => {});

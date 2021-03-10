import { createApp } from 'vue'
import EspAjaxLab from './EspAjaxLab.vue'

const app = createApp(EspAjaxLab);
const vm = app.mount('#app');

window.eal = vm;
window.live_controller = vm.$refs.LiveController;
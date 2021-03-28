import { createApp } from 'vue'
import EspAjaxLab from './EspAjaxLab.vue'
import router from './router'

const app = createApp(EspAjaxLab).use(router);
const vm = app.mount('#app');

window.eal = vm;
window.live_controller = vm.$refs.LiveController;
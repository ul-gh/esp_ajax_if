import { createApp } from 'vue'
import EspAjaxLab from './EspAjaxLab.vue'

const app = createApp(EspAjaxLab);
const vm = app.mount('#app');

document.vm = vm;
document.eal = vm.$refs.eal;

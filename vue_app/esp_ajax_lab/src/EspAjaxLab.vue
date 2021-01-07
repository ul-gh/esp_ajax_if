<template>
  <img src="./assets/logo.svg" alt="Application Logo" style="height: 3rem">
  <EspAjaxLabMain msg="Welcome to ESP AJAX Lab!" :disabled="disabled"/>
</template>

<script>
import EspAjaxLabMain from './components/EspAjaxLabMain.vue'

export default {
  name: 'EspAjaxLab',
  data(){
      return {
          disabled: undefined,
      }
  },
  components: {
    EspAjaxLabMain
  }
}
</script>

<style>
#app {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  text-align: center;
  color: #2c3e50;
  margin-top: 60px;
}
</style>

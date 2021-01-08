<template>
  <img src="./assets/logo.svg" alt="Application Logo" style="height: 3rem">
  <EspAjaxLabMain ref="eal" :disabled="$data.disabled"/>
</template>

<script>
import EspAjaxLabMain from './components/EspAjaxLabMain.vue'

export default {
  name: 'EspAjaxLab',
  data(){
      return {
          disabled: false,
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

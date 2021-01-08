<template>
  <link rel="icon" type="image/svg+xml" href='data:image/svg+xml;utf-8,
      <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 27.452 66.148">
        <path fill="red" d="M13.307 66.146l-2.897-16.43 3.2 4.572 4.042-23.543L0 40.936
            10.97 0h10.46L9.885 30.02l17.568-10.143-9.084 35.147 5.178-3.5-10.24 14.624"/>
      </svg>'/>
  <img alt="Application Logo" style="height: 3rem" src="./assets/logo.svg">

  <EspAjaxLabMain ref="eal"
      :remote_state="remote_state"
      :disabled="disabled"
      :view_updates_inhibited="view_updates_inhibited"
      @submit_cmd="submit_cmd"/>
</template>

<script>
import EspAjaxLabMain from './components/EspAjaxLabMain.vue'

export default {
  name: 'EspAjaxLab',
  components: {
    EspAjaxLabMain
  },
  data(){
    return {
      disabled: false,
        view_updates_inhibited: false,
        remote_state: {
          heatsink_temp: 88.8,
          aux_temp: 99.9,
          power_pwm_active: true,
          hw_oc_fault_occurred: false,
          hw_oc_fault_present: false,
        }
      }
  },
  methods: {
    submit_cmd(name, value) {
        
    }
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

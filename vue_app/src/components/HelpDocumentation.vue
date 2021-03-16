<!-- ESP Ajax Lab Help and Documentation Component
-->
<template>
  <div class="help_documentation">
      <h2>
          Online Help
      </h2>
      <h3>
          Online Documentation:
          <a href="https://ul-gh.github.io/esp_ajax_if/">
            https://ul-gh.github.io/esp_ajax_if/
          </a>
      </h3>
      <h3>
        Github Project:
        <a href="https://github.com/ul-gh/esp_ajax_if/">
            https://github.com/ul-gh/esp_ajax_if/
        </a>
      </h3>
  </div>
</template>

<script>
export default {
  name: "HelpDocumentation",
  components: {},
  data() {
    return {};
  },
  props: {
    state: Object,
    disabled: Boolean
  },
  emits: ["submit_cmd"]
};
</script>


<style>
h2 {
    text-align: center;
}
</style>

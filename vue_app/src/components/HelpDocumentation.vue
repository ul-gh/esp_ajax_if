<!-- ESP Ajax Lab Help and Documentation Component
-->
<template>
  <div class="help_documentation info_text">
    <h2>
      Online Help
    </h2>
    <br>
    <h3>
      Online Documentation:
      <a href="https://ul-gh.github.io/esp_ajax_if/">
        https://ul-gh.github.io/esp_ajax_if/
      </a>
    </h3>
    <br>
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
  emits: ["action"]
};
</script>


<style scoped>
.help_documentation {
  background-color: #e4f0f5;
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
}
h2 {
  text-align: center;
  margin: 0rem;
}
h3 {
  text-align: left;
  margin-left: 0.5rem;
  margin-bottom: 0rem;
}
a {
  text-align: left;
}
</style>

<!-- ESP-LiveControl Help and Documentation Component
-->
<template>
  <div class="help_documentation info_text">
    <h2>
      Online Help
    </h2>
    <br>
    <h3>
      <a href="https://ul-gh.github.io/esp_ajax_if/">
        Online Documentation Pages
      </a>
    </h3>
    <br>
    <h3>
      <a href="https://github.com/ul-gh/esp_ajax_if/">
        Github Project
      </a>
    </h3>
    <p><br>2021-04-02  Ulrich Lukas
    </p>
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
  emits: ["action_triggered"]
};
</script>


<style scoped>
.help_documentation {
  background-color: #e4f0f5;
  padding-left: 0.5rem;
}
h2 {
  text-align: center;
  margin: 0rem;
}
h3 {
  text-align: left;
  margin: 0rem;
}
a,p {
  text-align: left;
}
</style>

<template>
  <div class="esp_ajax_lab">
    <p>
      <a href="update.html">Perform OTA Firmware Upload</a>
    </p>
    <p>
        2021-03-11 Ulrich Lukas
    </p>
  </div>
</template>

<script>
export default {
  name: "UpdateNetworkSetup",
  components: {
  },
  data() {
    return {};
  },
  props: {
    state: Object,
    disabled: Boolean,
  },
  methods: {
    // Helper function for setting custom boolean attributes on any HTML element.
    // These are defined to be true when present and false when removed from the DOM.
    set_if(is_true) {
      return is_true ? "" : undefined;
    },
    // Push buttons can have a name and value
    submit_btn(event) {
      console.log(event);
      this.$emit("submit_cmd", event.target.name, event.target.value);
    },
    // Submit name=value pair
    submit_nv(name, value) {
        console.log("submitting " + name + "=" + value);
        this.$emit("submit_cmd", name, value);
    },
  },
  emits: ["submit_cmd"],
};
</script>


<style>
</style>

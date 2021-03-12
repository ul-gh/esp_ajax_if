<!-- ESP Ajax Lab Network and Updating Component
-->
<template>
  <div class="network_and_update">
    <h2>
      Over-The-Air Firmware Update
    </h2>
    <form action="/update" method="post" enctype="multipart/form-data">
      <table>
        <caption>
          Select a local ".bin" firmware file and click the "Perform Upload!"
          button to perform Over-The-Air flash update procedure for
          ESP Ajax Lab. Be aware, a wrong firmware will brick your device...
        </caption>
        <thead>
          <tr>
            <th>DANGER ZONE - OTA Firmware Upload</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><input type="file" name="update" /></td>
          </tr>
          <tr>
            <td>
              <input
                type="submit"
                class="button_large"
                value="Perform Upload!"
                id="btn_update"
              />
            </td>
          </tr>
        </tbody>
      </table>
    </form>
  </div>
</template>

<script>
export default {
  name: "NetworkAndUpdate",
  components: {},
  data() {
    return {};
  },
  props: {
    state: Object,
    disabled: Boolean
  },
  methods: {
    // Push buttons can have a name and value
    submit_btn(event) {
      console.log(event);
      this.$emit("submit_cmd", event.target.name, event.target.value);
    },
    // Submit name=value pair
    submit_nv(name, value) {
      console.log("submitting " + name + "=" + value);
      this.$emit("submit_cmd", name, value);
    }
  },
  emits: ["submit_cmd"]
};
</script>


<style>
</style>

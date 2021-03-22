<!-- ESP Ajax Lab Network and Updating Component
-->
<template>
  <div class="network_and_update">
    <form action="/update" method="post" enctype="multipart/form-data">
      <table>
        <caption class="info_text">
          <h2>
            Over-The-Air Firmware Update
          </h2>
          <p class="align-left">
            1.) Select a local ".bin" firmware file
          </p>
          <p class="align-left">
            2.) Click the "Perform Update Now!" button to start the upload/update
          </p>
          <p>
            ATTENTION: There is no integrity checks and a wrong firmware file will
            render the hardware non-functional or show unwanted behaviour!
          </p>
        </caption>
        <thead>
          <tr>
            <th>OTA Firmware Upload</th>
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
                value="Perform Update Now!"
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


<style scoped>
h2 {
  text-align: center;
}
caption {
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
}
.align-left {
  text-align: left;
  margin-left: 0.5rem;
}
</style>

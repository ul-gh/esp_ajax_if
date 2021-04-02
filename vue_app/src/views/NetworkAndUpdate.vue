<!-- ESP Ajax Lab Network and Updating Component
-->
<template>
  <div class="network_and_update">
    <form action="/network" method="get">
      <table>
        <caption class="info_text">
          <h2>
            WiFi / Network Settings
          </h2>
          <p class="align-left">
            You can set either access point mode or station mode.
          </p>
          <p class="align-left">
            For station mode, you can set a fixed IP or auto / DHCP. 
            Access point always has a static IP and runs a DHCP and DNS server 
            for which you can set a custom hostname.
          </p>
        </caption>
        <thead>
          <tr>
            <th colspan="2">Network Settings</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="mode_ap_station"
                    :value="'ap'"
                    v-model="mode_ap_station"
                >
                Access Point
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="mode_ap_station"
                    :value="'station'"
                    v-model="mode_ap_station"
                >
                Station Mode
              </label>
              </span>
            </td>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="mode_static_dhcp"
                    :value="'static'"
                    v-model="mode_static_dhcp"
                >
                Fixed IP
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="mode_static_dhcp"
                    :value="'dhcp'"
                    v-model="mode_static_dhcp"
                >
                Auto / DHCP
              </label>
              </span>
            </td>
          </tr>
          <tr>
            <th>DNS / Hostname</th>
            <th>WiFi Name/SSID</th>
          </tr>
          <tr>
            <td>
              <input
                type="text"
                v-model="hostname"
              >
            </td>
            <td>
              <input
                type="text"
                v-model="ssid"
              >
            </td>
          </tr>
          <tr>
            <th>IP Address</th>
            <th>Subnet Mask</th>
          </tr>
          <tr>
            <td>
              <input
                type="text"
                v-model="ipaddr"
            >
            </td>
            <td>
              <input
                type="text"
                v-model="netmask"
            >
            </td>
          </tr>
          <tr>
            <td colspan="2">
              <input
                type="submit"
                class="button_large"
                value="Submit!"
              />
            </td>
          </tr>
        </tbody>
      </table>
    </form>

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
    return {
      mode_ap_station: "ap",
      mode_static_dhcp: "static",
      hostname: "eal",
      ssid: "esp-ajax-lab",
      ipaddr: "192.168.4.1",
      netmask: "255.255.0.0",
    };
  },
  props: {
    state: Object,
    disabled: Boolean
  },
  methods: {
    // Push buttons can have a name and value
    dispatch_btn(event) {
      this.$emit("action", event.target.name, event.target.value);
    },
    // Submit name=value pair
    dispatch_nv(name, value) {
      this.$emit("action", name, value);
    }
  },
  emits: ["action"]
};
</script>


<style scoped>
h2 {
  text-align: center;
}

/* caption {
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
} */

label {
  font-weight: bold;
}

input[type="text"] {
  width: 13rem;
}

.align-left {
  text-align: left;
  margin-left: 0.5rem;
}
</style>

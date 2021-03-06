import { createRouter, createWebHistory } from 'vue-router'
import LiveController from "@/views/LiveController.vue";
import OperationSettings from "@/views/OperationSettings.vue";
import HelpDocumentation from "@/views/HelpDocumentation.vue";
import NetworkAndUpdate from "@/views/NetworkAndUpdate.vue";

const routes = [
  {
    path: '/app',
    name: 'Live Control',
    component: LiveController,
  },
  {
    path: '/app/Settings',
    name: 'Settings',
    component: OperationSettings,
    // route level code-splitting
    // this generates a separate chunk (about.[hash].js) for this route
    // which is lazy-loaded when the route is visited.
    //component: () => import(/* webpackChunkName: "about" */ '../views/About.vue')
  },
  {
    path: '/app/Documentation',
    name: 'Documentation',
    component: HelpDocumentation,
  },
  {
    path: '/app/NetworkAndUpdate',
    name: 'Network / Update',
    component: NetworkAndUpdate,
  },
  {
    // Catch-all route
    path: '/:pathMatch(.*)*',
    redirect: '/app',
  }
]

const router = createRouter({
  //base: "/app/",
  history: createWebHistory(process.env.BASE_URL),
  routes
})

export default router

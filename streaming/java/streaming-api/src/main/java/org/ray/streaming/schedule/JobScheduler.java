package org.ray.streaming.schedule;


import java.util.Map;

import org.ray.streaming.plan.Plan;

/**
 * Interface of the job scheduler.
 */
public interface JobScheduler {

  /**
   * Assign logical plan to physical execution graph, and schedule job to run.
   *
   * @param plan The logical plan.
   */
  void schedule(Plan plan, Map<String, String> conf);
}

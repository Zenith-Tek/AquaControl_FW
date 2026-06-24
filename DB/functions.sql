-- RPC function for reporting device crashes
CREATE OR REPLACE FUNCTION public.report_crash(
  p_device_id text,
  p_firmware_version text,
  p_reboot_reason text,
  p_crashed_task text DEFAULT NULL,
  p_fault_pc text DEFAULT NULL,
  p_backtrace text DEFAULT NULL,
  p_uptime_before_crash bigint DEFAULT NULL
)
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
BEGIN
  INSERT INTO public.crash_logs (
    device_id,
    firmware_version,
    reboot_reason,
    crashed_task,
    fault_pc,
    backtrace,
    uptime_before_crash
  ) VALUES (
    p_device_id,
    p_firmware_version,
    p_reboot_reason,
    p_crashed_task,
    p_fault_pc,
    p_backtrace,
    p_uptime_before_crash
  );
END;
$$;

-- RPC function for reporting OTA progress/failures
CREATE OR REPLACE FUNCTION public.report_ota_status(
  p_device_id text,
  p_from_version text DEFAULT NULL,
  p_to_version text DEFAULT NULL,
  p_phase text DEFAULT NULL,
  p_error text DEFAULT NULL,
  p_attempt integer DEFAULT NULL
)
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
BEGIN
  INSERT INTO public.ota_status (
    device_id,
    from_version,
    to_version,
    phase,
    error,
    attempt
  ) VALUES (
    p_device_id,
    p_from_version,
    p_to_version,
    p_phase,
    p_error,
    p_attempt
  );
END;
$$;

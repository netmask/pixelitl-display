defmodule StudioWeb.PageController do
  use StudioWeb, :controller

  def home(conn, _params) do
    redirect(conn, to: ~p"/simulator")
  end
end
